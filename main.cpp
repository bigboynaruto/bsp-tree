#include <sstream>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <string>

#include <CGAL/Polyhedron_3.h>
#include <CGAL/IO/Polyhedron_iostream.h>
#include <CGAL/convex_hull_3.h>
#include <CGAL/enum.h>
#include <CGAL/Nef_polyhedron_3.h>
#include <CGAL/IO/Nef_polyhedron_iostream_3.h>
#include <CGAL/OFF_to_nef_3.h>
#include <CGAL/convex_decomposition_3.h>

#include "bsp.h"

struct MenuData {
    BSPTree bsp;
    std::map<int, Polyhedron_3> polys;
};

const std::string HELP = "h",
      EXIT = "q",
      LOCATE = "loc",
      NEW = "new",
      ADD = "add",
      CLEAR = "cl",
      REMOVE = "rm",
      PRINT = "out";

void help();
bool again();
void locate(std::istringstream&, const MenuData&);
void new_bsp(std::istringstream&, MenuData&);
void add(std::istringstream&, MenuData&);
void rm(std::istringstream&, MenuData&);
void out(std::istringstream&, const MenuData&);
Polyhedron_3 cube(const Point_3&, const CGAL::Gmpq&,
        const CGAL::Gmpq&, const CGAL::Gmpq&);
void cubes(unsigned int, unsigned int, unsigned int,
        std::vector<Polyhedron_3>&);

int main(int argc, char **argv) {
    std::cout << "Welcome! Use `h` for help." << std::endl;

    std::string line;
    MenuData md;
    while ((std::cout << "> ") && std::getline(std::cin, line)) {
        if (line.empty())
            continue;

        std::istringstream iss(line);

        std::string command;
        iss >> command;

        if (command == HELP)
            help();
        else if (command == EXIT)
            break;
        else if (command == LOCATE) {
            locate(iss, md);
        }
        else if (command == NEW) {
            new_bsp(iss, md);
        }
        else if (command == ADD) {
            add(iss, md);
        }
        else if (command == REMOVE) {
            rm(iss, md);
        }
        else if (command == PRINT) {
            out(iss, md);
        }
        else if (command == CLEAR) {
            if (again()) {
                md.bsp.clear();
                md.polys.clear();
                std::cout << "Cleared." << std::endl;
            }
        }
        else std::cout << "Use `h` for help." << std::endl;
    }

    std::cout << "Clearing BSP tree... " << std::flush;
    md.bsp.clear();
    std::cout << "Done." << std::endl;
}

void help() {
    std::cout << "Print this message:" << std::endl
        << "  " << HELP << std::endl
        << "Locate point in BSP tree:" << std::endl
        << "  " << LOCATE << " x y z [filename]" << std::endl
        << "Create new BSP tree:" << std::endl
        << "  " << NEW << " h w d" << std::endl
        << "  " << NEW << " filename" << std::endl
        << "Add new convex polyhedron:" << std::endl
        << "  " << ADD << " [filename]" << std::endl
        << "Remove polyhedron from BSP tree:" << std::endl
        << " " << REMOVE << " id" << std::endl
        << "Clear BSP tree:" << std::endl
        << "  " << CLEAR << std::endl
        << "Print BSP tree:" << std::endl
        << "  " << PRINT << std::endl
        << "Exit the program:" << std::endl
        << "  " << EXIT << std::endl;
}

bool again() {
    std::string line;
    std::cout << "Are you sure? (Y/n) ";
    std::getline(std::cin, line);
    return !line.empty() && line[0] == 'Y';
}

void error(const std::string &msg) {
    std::cout << "ERROR: " << msg << std::endl;
}

void out(std::istringstream &iss, const MenuData &md) {
    std::string filename;
    std::ofstream ofs;
    if (!(iss >> filename) || !(ofs = std::ofstream(filename.c_str()))) {
        std::cout << md.bsp << std::endl;
    }

    ofs << md.bsp << std::endl;
}

void locate(std::istringstream &iss, const MenuData &md) {
    Point_3 p;
    if (!(iss >> p)) {
        error("Invalid input!");
        std::cout << "Usage: " << LOCATE << " x y z [filename]" << std::endl
            << "  x, y, z - coordinates of point" << std::endl
            << "  filename - name of the .off file to send output to" << std::endl;
        return;
    }

    Polyhedron_3 poly;
    if (!md.bsp.locate(p, poly)) {
        std::cout << "Location failed!" << std::endl;
        return;
    }

    std::cout << "Located in Polyhedron#" << poly.id() << std::endl;

    std::string filename;
    if (iss >> filename) {
        std::ofstream fout(filename.c_str());

        if (!fout) {
            error("Cannot open file '" + filename + "'!");
        }
        else {
            std::cout << "Outputting to file '" << filename << "'...";
            fout << poly;
            std::cout << " Done." << std::endl;
            return;
        }
    }

    std::cout << "Outputting to console..." << std::endl;
    std::cout << poly << std::endl;
    std::cout << "Done." << std::endl;
}

void new_bsp(std::istringstream &iss, MenuData &md) {
    int h, w, d;
    if (iss >> h >> w >> d) {
        std::cout << "Building " << h * w * d << " cubes... " << std::flush;
        std::vector<Polyhedron_3> polys;
        cubes(h, w, d, polys);
        std::cout << "Done." << std::endl;
        std::random_shuffle(polys.begin(), polys.end());

        try {
            std::cout << "Building BSP tree... " << std::flush;
            md.bsp = BSPTree(polys);
            std::cout << "Done." << std::endl;

            md.polys.clear();
            for (const Polyhedron_3 &poly : polys) {
                md.polys[poly.id()] = poly;
            }
        } catch (std::runtime_error e) {
            error(e.what());
        }

        return;
    }

    iss.clear();
    std::string filename;
    if (!(iss >> filename)) {
        error("Invalid input!");
        std::cout << "Usage: " << NEW << " (h w d | filename)" << std::endl
            << "  h w d - height, width and depth of cube" << std::endl
            << "  filename - name of the .off file" << std::endl;
        return;
    }

    std::ifstream ifs(filename.c_str());
    if (!ifs) {
        error("Cannot open file '" + filename + "'!");
        return;
    }

    typedef CGAL::Nef_polyhedron_3<K> Nef_3;
    typedef Nef_3::Volume_const_iterator Volume_const_iterator;

    Nef_3 N;
    std::cout << "Reading Nef Polyhedron from .off file... " << std::flush;
    std::size_t discarded = CGAL::OFF_to_nef_3 (ifs, N, true);
    std::cout << "Done." << std::endl;

    //std::cout << "Press Enter to proceed... ";
    //std::getline(std::cin, filename);

    std::cout << "  Nef vertices: "
        << N.number_of_vertices() << std::endl;
    std::cout << "  Nef edges: "
        << N.number_of_edges() << std::endl;
    std::cout << "  Nef facets: "
        << N.number_of_facets() << std::endl;
    std::cout << "  Nef volumes: "
        << N.number_of_volumes() << std::endl;
    std::cout << "  number of discarded facets: "
        << discarded << std::endl;

    std::cout << "Building convex decomposition... " << std::flush;
    CGAL::convex_decomposition_3(N);
    std::cout << "Done." << std::endl;

    std::vector<Polyhedron_3> convex_parts;
    // the first volume is the outer volume, which is
    // ignored in the decomposition
    Volume_const_iterator ci = ++N.volumes_begin();
    for( ; ci != N.volumes_end(); ++ci) {
        if(ci->mark()) {
            Polyhedron_3 P;
            N.convert_inner_shell_to_polyhedron(ci->shells_begin(), P);
            std::transform(P.facets_begin(), P.facets_end(), P.planes_begin(),
                    Plane_equation());
            convex_parts.push_back(P);
        }
    }

    std::random_shuffle(convex_parts.begin(), convex_parts.end());
    std::cout << "Decomposition into " << convex_parts.size() << " convex parts " << std::endl;

    md.polys.clear();

    //std::cout << "Press Enter to proceed... ";
    //std::getline(std::cin, filename);

    std::cout << "Building BSP tree... " << std::flush;
    try {
        md.bsp = BSPTree(convex_parts);
        std::cout << "Done." << std::endl;
        for (const Polyhedron_3 &poly : convex_parts) {
            md.polys[poly.id()] = poly;
        }
    } catch (std::runtime_error e) {
        error(e.what());
        std::cout << "Clearing BSP tree..." << std::endl;
        md.bsp.clear();
        std::cout << "Done." << std::endl;
    }
}

void add(std::istringstream &iss, MenuData &md) {
    std::vector<Point_3> points;
    std::string filename;
    std::ifstream ifs;
    if (iss >> filename && (ifs = std::ifstream(filename.c_str()))) {
        Point_3 p;
        while (ifs >> p)
            points.push_back(p);
    }
    else {
        std::cout << "Enter a series of `x y z` coordinates "
            << "followed by an empty line." << std::endl;

        std::string line;
        Point_3 p;
        while ((std::cout << ": ") && std::getline(std::cin, line)) {
            std::istringstream _iss(line);
            if (!(_iss >> p))
                break;
            points.push_back(p);
        }
    }

    Polyhedron_3 P;
    CGAL::convex_hull_3(points.begin(), points.end(), P);

    std::cout << "The convex hull contains " << P.size_of_vertices() << " vertices" << std::endl;
    std::transform(P.facets_begin(), P.facets_end(), P.planes_begin(),
            Plane_equation());

    try {
        std::cout << "Adding Polyhedron#" << P.id() << "... " << std::flush;
        if (md.bsp.insert(P)) {
            std::cout << "Done." << std::endl;
            md.polys.insert(std::make_pair(P.id(), P));
        }
        else {
            error("Cannot add polyhedron due to its invalidity or various other reasons.");
        }
    } catch (std::runtime_error e) {
        error(e.what());
    }
}

void rm(std::istringstream &iss, MenuData &md) {
    int id;
    if (!(iss >> id)) {
        error("Invalid input!");
        std::cout << "Usage: " << REMOVE << " id" << std::endl;
        return;
    }

    auto iter = md.polys.find(id);
    if (iter == md.polys.end()) {
        error("Cannot find id=" + std::to_string(id) + "!");
        return;
    }

    std::cout << "Removing Polyhedron#" << iter->second.id() << "... " << std::flush;
    if (md.bsp.remove(iter->second)) {
        std::cout << "Done." << std::endl;
        md.polys.erase(iter);
    }
    else {
        error("Cannot remove polyhedron.");
    }
}


Polyhedron_3 cube(const Point_3 &p, const CGAL::Gmpq &h,
        const CGAL::Gmpq &w, const CGAL::Gmpq &d) {
    std::vector<Point_3> points;
    for (auto &x: {p.x(), p.x() + h})
        for (auto &y: {p.y(), p.y() + w})
            for (auto &z: {p.z(), p.z() + d})
                points.push_back(Point_3(x, y, z));

    Polyhedron_3 P;
    CGAL::convex_hull_3(points.begin(), points.end(), P);
    std::transform(P.facets_begin(), P.facets_end(), P.planes_begin(),
            Plane_equation());

    return P;

}

void cubes(unsigned int hn, unsigned int wn, unsigned int dn,
        std::vector<Polyhedron_3> &v) {
    v.clear();
    for (int x = 0; x <= hn; ++x) {
        for (int y = 0; y <= wn; ++y) {
            for (int z = 0; z <= dn; ++z) {
                v.push_back(cube(Point_3(x, y, z), 1, 1, 1));
            }
        }
    }
}
