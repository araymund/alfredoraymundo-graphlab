#ifndef GRAPHLAB_GRAPH_BASIC_TYPES
#define GRAPHLAB_GRAPH_BASIC_TYPES

#include <stdint.h>

namespace graphlab {
  /// The type of a vertex 
  typedef uint32_t vertex_id_type;
  
  /// The type of an edge (to deprecate)
  typedef uint32_t edge_id_type;
  
  /// Type for vertex colors 
  typedef vertex_id_type vertex_color_type;

  /**
   * The set of edges that are operated on during gather and scatter
   * operations.
   */
  enum edge_set {IN_EDGES, OUT_EDGES, ALL_EDGES, NO_EDGES};
} // end of namespace graphlab

#endif
