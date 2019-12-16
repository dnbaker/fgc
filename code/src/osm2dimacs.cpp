/*

  EXAMPLE osmium_road_length

  Calculate the length of the road network (everything tagged `highway=*`)
  from the given OSM file.

  DEMONSTRATES USE OF:
  * file input
  * location indexes and the NodeLocationsForWays handler
  * length calculation on the earth using the haversine function

  SIMPLER EXAMPLES you might want to understand first:
  * osmium_read
  * osmium_count
  * osmium_pub_names

  LICENSE
  The code in this example file is released into the Public Domain.

*/

#include <cstdlib>  // for std::exit
#include <iostream> // for std::cout, std::cerr
#include <cstdio>   // std::FILE *

// Allow any format of input files (XML, PBF, ...)
#include <osmium/io/any_input.hpp>

// For the osmium::geom::haversine::distance() function
#include <osmium/geom/haversine.hpp>

// For osmium::apply()
#include <osmium/visitor.hpp>

// For the location index. There are different types of indexes available.
// This will work for all input files keeping the index in memory.
#include <osmium/index/map/flex_mem.hpp>

// For the NodeLocationForWays handler
#include <osmium/handler/node_locations_for_ways.hpp>
#include <unordered_set>
#include <vector>

// The type of index used. This must match the include file above
using index_type = osmium::index::map::FlexMem<osmium::unsigned_object_id_type, osmium::Location>;

// The location handler always depends on the index type
using location_handler_type = osmium::handler::NodeLocationsForWays<index_type>;

// This handler only implements the way() function, we are not interested in
// any other objects.
using id_int_t = osmium::object_id_type;
struct RoadLengthHandler : public osmium::handler::Handler {

    double length = 0;
    std::unordered_set<id_int_t> node_ids_;
    struct EdgeD {
        id_int_t lhs_, rhs_;
        double dist_;
    };
    std::vector<EdgeD> edges_;

    // If the way has a "highway" tag, find its length and add it to the
    // overall length.
    void way(const osmium::Way& way) {
        //const char* highway = way.tags()["highway"];
        if(way.tags()["highway"] == nullptr) return;
        length += osmium::geom::haversine::distance(way.nodes());
        auto &nodes = way.nodes();
        node_ids_.insert(nodes[nodes.size() - 1].ref());
        for(size_t i = 0, e = nodes.size() - 1; i < e; ++i) {
            auto id = nodes[i].ref();
            node_ids_.insert(id);
            //auto dist = osmium::geom::haversine::distance(nodes[i], nodes[i + 1]);
            auto dist = osmium::geom::haversine::distance(nodes[i].location(), nodes[i + 1].location());
            EdgeD ed{id, nodes[i + 1].ref(), dist};
            edges_.emplace_back(ed);
        }
        
    }

}; // struct RoadLengthHandler

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " OSMFILE\n";
        std::exit(1);
    }
    std::FILE *ofp = argc < 3 ? stdout: std::fopen(argv[2], "w");

    try {
        // Initialize the reader with the filename from the command line and
        // tell it to only read nodes and ways.
        osmium::io::Reader reader{argv[1], osmium::osm_entity_bits::node | osmium::osm_entity_bits::way};

        // The index to hold node locations.
        index_type index;

        // The location handler will add the node locations to the index and then
        // to the ways
        location_handler_type location_handler{index};

        // Our handler defined above
        RoadLengthHandler road_length_handler;

        // Apply input data to first the location handler and then our own handler
        osmium::apply(reader, location_handler, road_length_handler);
#if 0
        std::vector<id_int_t> ids;
        ids.reserve(location_handler.node_ids_.size());
#endif
        id_int_t assigned_id = 0;
        std::unordered_map<id_int_t, id_int_t> reassigner;
        reassigner.reserve(road_length_handler.node_ids_.size());
        std::fprintf(ofp, "c Auto-generated 9th DIMACS Implementation Challenge: Shortest Paths-format file\n"
                          "c From Open Street Maps [OSM] (https://openstreetmap.org)\n"
                          "c Using libosmium\n"
                          "c Following this line are node reassignments from ids to parsed node ids, all marked as comments lines.\n"
                          "p sp %zu %zu\n",
                     road_length_handler.node_ids_.size(), road_length_handler.edges_.size());
        for(const auto id: road_length_handler.node_ids_) {
            reassigner[id] = assigned_id;
            std::fprintf(ofp, "c %lld->%lld\n", id, assigned_id);
            ++assigned_id;
        }
        for(const auto &edge: road_length_handler.edges_) {
            auto lhs = reassigner[edge.lhs_], rhs = reassigner[edge.rhs_];
            std::fprintf(ofp, "a %lld %lld %g\n", lhs, rhs, edge.dist_);
        }

        // Output the length. The haversine function calculates it in meters,
        // so we first devide by 1000 to get kilometers.
        std::cout << "Length: " << road_length_handler.length / 1000 << " km\n";
    } catch (const std::exception& e) {
        // All exceptions used by the Osmium library derive from std::exception.
        std::cerr << e.what() << '\n';
        std::exit(1);
    }
    if(ofp != stdout) std::fclose(ofp);
}

