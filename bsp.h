#ifndef BSP_H
#define BSP_H

#include <ostream>

#include <CGAL/Gmpq.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Polyhedron_3.h>

class Node;

typedef CGAL::Simple_cartesian<CGAL::Gmpq> K;
typedef K::Point_3 Point_3;
typedef K::Plane_3 Plane_3;
typedef CGAL::Polyhedron_3<K> CGALPolyhedron_3;

struct Plane_equation {
    template <class Facet>
    typename Facet::Plane_3 operator()( Facet& f) {
        typename Facet::Halfedge_handle h = f.halfedge();
        typedef typename Facet::Plane_3  Plane;
        return Plane( h->vertex()->point(),
                      h->next()->vertex()->point(),
                      h->next()->next()->vertex()->point());
    }
};

class Polyhedron_3 : public CGAL::Polyhedron_3<K> {
    int _id;
    static int _max_id;

    public:

        Polyhedron_3();
        Polyhedron_3(const CGALPolyhedron_3&);

        bool operator==(const Polyhedron_3&) const;
        bool operator<(const Polyhedron_3&) const;

        int id() const;
};

class BSPTree {
    Node *root;

    public:

        BSPTree();
        BSPTree(std::initializer_list<Polyhedron_3>);
        BSPTree(const std::vector<Polyhedron_3>&);

        BSPTree(const BSPTree&) = delete;
        BSPTree(BSPTree&&);
        BSPTree& operator=(BSPTree&&);
        BSPTree& operator=(const BSPTree&) = delete;

        ~BSPTree();

        bool locate(const Point_3&, Polyhedron_3&) const;
        bool insert(const Polyhedron_3&);
        bool remove(const Polyhedron_3&);

        bool empty() const;
        void clear();

        friend std::ostream& operator<<(std::ostream&, const BSPTree&);
};

bool point_in_polyhedron(const Polyhedron_3&, const Point_3&);

#endif // BSP_H
