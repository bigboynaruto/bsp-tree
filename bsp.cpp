#include <cstdlib>
#include <stack>
#include <algorithm>
#include <iomanip>
#include <ostream>

#include <CGAL/enum.h>

#include "bsp.h"

struct PointInPolyhedron;
class InternalNode;
class LeafNode;

enum OrientedSide {
    ON_NEGATIVE_SIDE = 1 << 1, // CGAL::ON_NEGATIVE_SIDE,
    ON_ORIENTED_BOUNDARY = 1 << 2, // CGAL::ON_ORIENTED_BOUNDARY,
    ON_POSITIVE_SIDE = 1 << 3, // CGAL::ON_POSITIVE_SIDE,
};

int oriented_side(const Plane_3&, const Polyhedron_3&);
Node* split(const Polyhedron_3&, const Polyhedron_3&);
Node* create_node(const std::vector<Polyhedron_3>&);
std::ostream& print(std::ostream&, Node*, int);

struct PointInPolyhedron {
    Point_3 p;

    PointInPolyhedron(const Point_3 &_p)
        : p(_p) {}

    bool operator()(const Polyhedron_3 &poly) {
        return point_in_polyhedron(poly, p);
    }
};

// CLASS Polyhedron_3

int Polyhedron_3::_max_id = 0;

Polyhedron_3::Polyhedron_3()
        : _id(_max_id++) {}

Polyhedron_3::Polyhedron_3(const CGALPolyhedron_3 &poly)
        : CGALPolyhedron_3(poly), _id(_max_id++) {}

bool Polyhedron_3::operator==(const Polyhedron_3 &other) const {
    return other._id == _id;
}

bool Polyhedron_3::operator<(const Polyhedron_3 &other) const {
    return _id < other._id;
}

int Polyhedron_3::id() const {
    return _id;
}

// CLASS Node

class Node {
    public:
        InternalNode *parent;

        virtual ~Node() {}
        virtual bool has_children() = 0;
    protected:
        Node(InternalNode *_parent = NULL) : parent(_parent) {}
};

class InternalNode : public Node {
    public:
        Node *left, *right;
        Plane_3 plane;
        std::set<Polyhedron_3> polys;

        InternalNode(Node *_left, Node *_right, const Plane_3 &_plane, std::set<Polyhedron_3> _polys, InternalNode *_parent = NULL)
                : Node(_parent), left(_left), right(_right), plane(_plane), polys(_polys) {
            left->parent = this;
            right->parent = this;
        }

        bool has_children() {
            return true;
        }
};

class LeafNode : public Node {
    public:
        Polyhedron_3 poly;

        LeafNode(const Polyhedron_3 &_poly, InternalNode *_parent = NULL)
            : Node(_parent), poly(_poly) {}

        bool has_children() {
            return false;
        }
};

// CLASS BSPTree

BSPTree::BSPTree() : BSPTree({}) {}

BSPTree::BSPTree(std::initializer_list<Polyhedron_3> il) : BSPTree(std::vector<Polyhedron_3>(il)) {}

BSPTree::BSPTree(const std::vector<Polyhedron_3> &v) : root(::create_node(v)) {}

BSPTree::BSPTree(BSPTree &&other) {
    Node *other_root = other.root;
    other.root = root;
    root = other_root;
}

BSPTree& BSPTree::operator=(BSPTree &&other) {
    Node *other_root = other.root;
    other.root = root;
    root = other_root;

    return *this;
}

BSPTree::~BSPTree() {
    clear();
}

bool BSPTree::locate(const Point_3 &p, Polyhedron_3 &poly) const {
    if (empty())
        return false;

    Node *node = root;
    while (node->has_children()) {
        InternalNode *inode = static_cast<InternalNode*>(node);

        switch (inode->plane.oriented_side(p)) {
            case CGAL::ON_POSITIVE_SIDE:
                node = inode->right;
                break;
            case CGAL::ON_NEGATIVE_SIDE:
                node = inode->left;
                break;
            case CGAL::ON_ORIENTED_BOUNDARY:
                goto point_on_plane;
        }
    }

    point_on_plane:
    if (node->has_children()) {
        InternalNode *inode = static_cast<InternalNode*>(node);
        auto found = std::find_if(inode->polys.begin(), inode->polys.end(), PointInPolyhedron(p));

        if (found == inode->polys.end())
            return false;

        poly = *found;
        return true;
    }

    LeafNode *lnode = static_cast<LeafNode*>(node);
    if (point_in_polyhedron(lnode->poly, p)) {
        poly = lnode->poly;
        return true;
    }

    return false;
}

bool insert(Node *node, const Polyhedron_3 &poly) {
    while (node->has_children()) {
        int side = oriented_side(static_cast<InternalNode*>(node)->plane, poly),
            l = side & ON_NEGATIVE_SIDE,
            r = side & ON_POSITIVE_SIDE,
            z = side & ON_ORIENTED_BOUNDARY;
        if (l && r)
            break;
        if (l) node = static_cast<InternalNode*>(node)->left;
        else node = static_cast<InternalNode*>(node)->right;
    }

    if (node->has_children()) {
        InternalNode *inode = static_cast<InternalNode*>(node);
        return insert(inode->left, poly) & insert(inode->right, poly);
    }

    LeafNode *lnode = static_cast<LeafNode*>(node);
    if (lnode->parent->left == lnode)
        lnode->parent->left = split(poly, lnode->poly);
    else lnode->parent->right = split(poly, lnode->poly);

    delete lnode;

    return true;
}

bool BSPTree::insert(const Polyhedron_3 &poly) {
    if (!poly.is_valid())
        return false;

    if (!root) {
        root = new LeafNode(poly);
    }
    else if (!root->has_children()) {
        Node *tmp = root;
        root = split(static_cast<LeafNode*>(root)->poly, poly);
        delete tmp;
    }
    else return ::insert(root, poly);

    return true;
}

bool remove(Node *node, const Polyhedron_3 &poly, Node *&root) {
    if (!node)
        return false;

    while (node->has_children()) {
        InternalNode *inode = static_cast<InternalNode*>(node);
        int side = oriented_side(inode->plane, poly),
            l = side & ON_NEGATIVE_SIDE,
            r = side & ON_POSITIVE_SIDE,
            z = side & ON_ORIENTED_BOUNDARY;

        if (z)
            inode->polys.erase(poly);
        if (l && r)
            return remove(inode->left, poly, root) | remove(inode->right, poly, root);

        if (l) node = inode->left;
        else node = inode->right;
    }

    LeafNode *lnode = static_cast<LeafNode*>(node);
    if (!(lnode->poly == poly))
        return false;

    Node *new_child = lnode->parent->left == lnode ? lnode->parent->right : lnode->parent->right;
    InternalNode *parent = static_cast<InternalNode*>(lnode->parent->parent);
    new_child->parent = parent;

    if (lnode->parent->parent) {
        if (parent->left == lnode->parent)
            parent->left = new_child;
        else parent->right = new_child;
    }
    else root = new_child;

    delete lnode->parent;
    delete lnode;

    return true;
}

bool BSPTree::remove(const Polyhedron_3 &poly) {
    if (!root)
        return false;

    if (root->has_children())
        return ::remove(root, poly, root);

    if (!(poly == static_cast<LeafNode*>(root)->poly))
        return false;

    delete root;
    root = NULL;

    return true;
}

bool BSPTree::empty() const {
    return !root;
}

void BSPTree::clear() {
    std::stack<Node*> nodes;
    if (root)
        nodes.push(root);
    while (!nodes.empty()) {
        Node *node = nodes.top();
        nodes.pop();

        if (node->has_children()) {
            InternalNode *inode = static_cast<InternalNode*>(node);
            nodes.push(inode->left);
            nodes.push(inode->right);
        }

        delete node;
    }

    root = NULL;
}

std::ostream& operator<<(std::ostream &out, const BSPTree &t) {
    print(out, t.root, 0);
}

// FUNCTIONS

bool point_in_polyhedron(const Polyhedron_3 &poly, const Point_3 &p) {
    for (auto it_plane = poly.planes_begin(); it_plane != poly.planes_end(); ++it_plane) {
        CGAL::Oriented_side side = it_plane->oriented_side(p);
        if (side == CGAL::ON_ORIENTED_BOUNDARY)
            continue;

        for (auto it_p = poly.points_begin(); it_p != poly.points_end(); ++it_p) {
            CGAL::Oriented_side _side = it_plane->oriented_side(*it_p);
            if (_side != CGAL::ON_ORIENTED_BOUNDARY && _side != side)
                return false;
        }
    }

    return true;
}

int oriented_side(const Plane_3 &plane, const Polyhedron_3 &poly) {
    int res = 0, max = ON_NEGATIVE_SIDE + ON_ORIENTED_BOUNDARY + ON_POSITIVE_SIDE;
    for (auto it = poly.points_begin(); it != poly.points_end() && res < max; ++it) {
        switch (plane.oriented_side(*it)) {
            case CGAL::ON_NEGATIVE_SIDE:
                res |= ON_NEGATIVE_SIDE;
                break;
            case CGAL::ON_ORIENTED_BOUNDARY:
                res |= ON_ORIENTED_BOUNDARY;
                break;
            case CGAL::ON_POSITIVE_SIDE:
                res |= ON_POSITIVE_SIDE;
        }
    }

    return res;
}

Node* split(const Polyhedron_3 &poly1, const Polyhedron_3 &poly2) {
    std::set<Polyhedron_3> polys = { poly1 };
    for (auto it = poly1.planes_begin(); it != poly1.planes_end(); ++it) {
        int side1 = oriented_side(*it, poly1),
            side2 = oriented_side(*it, poly2);
        if (side1 == side2 || side1 - ON_ORIENTED_BOUNDARY == side2 || side1 == side2 - ON_ORIENTED_BOUNDARY)
            continue;

        switch (side2) {
            case ON_NEGATIVE_SIDE + ON_ORIENTED_BOUNDARY:
                polys.insert(poly2);
            case ON_NEGATIVE_SIDE:
                return new InternalNode(new LeafNode(poly2), new LeafNode(poly1), *it, polys);
            case ON_POSITIVE_SIDE + ON_ORIENTED_BOUNDARY:
                polys.insert(poly2);
            case ON_POSITIVE_SIDE:
                return new InternalNode(new LeafNode(poly1), new LeafNode(poly2), *it, polys);
        }
    }

    polys = { poly2 };
    for (auto it = poly2.planes_begin(); it != poly2.planes_end(); ++it) {
        int side1 = oriented_side(*it, poly1),
            side2 = oriented_side(*it, poly2);
        if (side2 == side1 || side2 - ON_ORIENTED_BOUNDARY == side1 || side2 == side1 - ON_ORIENTED_BOUNDARY)
            continue;

        switch (side1) {
            case ON_NEGATIVE_SIDE + ON_ORIENTED_BOUNDARY:
                polys.insert(poly1);
            case ON_NEGATIVE_SIDE:
                return new InternalNode(new LeafNode(poly1), new LeafNode(poly2), *it, polys);
            case ON_POSITIVE_SIDE + ON_ORIENTED_BOUNDARY:
                polys.insert(poly1);
            case ON_POSITIVE_SIDE:
                return new InternalNode(new LeafNode(poly2), new LeafNode(poly1), *it, polys);
        }
    }

    //return new LeafNode(poly1);
    throw std::runtime_error("Intersecting polyhedrons!");
}

Node* create_node(const std::vector<Polyhedron_3> &v) {
    int size = v.size();
    switch (size) {
        case 0:
            return NULL;
        case 1:
            return new LeafNode(v[0]);
        case 2:
            return split(v[0], v[1]);
    }

    std::vector<Polyhedron_3> left, right;
    std::vector<Polyhedron_3> polys;
    Plane_3 plane;
    for (const Polyhedron_3 &poly: v) {
        //const Polyhedron_3 &poly = v[rand() % v.size()];
        for (auto plane_it = poly.planes_begin(); plane_it != poly.planes_end(); ++plane_it) {
            int side = oriented_side(*plane_it, poly);

            left.clear();
            right.clear();
            polys.clear();

            for (const Polyhedron_3 &poly: v) {
                int side = oriented_side(*plane_it, poly);
                if (side & ON_NEGATIVE_SIDE)
                    left.push_back(poly);
                if (side & ON_POSITIVE_SIDE)
                    right.push_back(poly);
                if (side & ON_ORIENTED_BOUNDARY)
                    polys.push_back(poly);
            }

            if (left.size() && left.size() < size && right.size() && right.size() < size) {
                plane = *plane_it;
                goto found_divider;
            }
        }
    }

found_divider:
    Node *node_left = create_node(left);
    left.clear();
    Node *node_right = create_node(right);
    right.clear();
    return new InternalNode(node_left, node_right, plane, std::set<Polyhedron_3>(polys.begin(), polys.end()));
}

std::ostream& print(std::ostream &out, Node *node, int depth) {
    if (!node)
        return out;
    if (!node->has_children())
        return out << std::setw(depth) << std::setfill('+') << ""
            << static_cast<LeafNode*>(node)->poly.id() << std::endl;
    InternalNode *inode = static_cast<InternalNode*>(node);
    print(out, inode->left, depth+1);
    return print(out, inode->right, depth+1);
}
