// parallel implementation of the k-means++ algorithm
// see: http://en.wikipedia.org/wiki/K-means%2B%2B
// written by Danny Bickson, CMU


#include "clustering.h"
#include "../gabp/advanced_config.h"

extern advanced_config ac;
extern problem_setup ps;

graph_type_kcores * g = NULL;

using namespace std;

void calc_initial_degree(){
  g = ps.g<graph_type_kcores>();
  for (int i=0; i<ps.M; i++){
     kcores_data & data = g->vertex_data(i);
     data.degree = g->out_edge_ids(i).size();
  }
}

void last_iter_kcores(){
  int num_active = 0;
  for (int i=0; i<ps.M+ps.N; i++){
    if (g->vertex_data(i).active)
	num_active++;
  }
  printf("Number of active nodes in round %d is %d\n", ps.iiter, num_active);
}

void kcores_update_function(gl_types_kcores::iscope & scope, gl_types_kcores::icallback & scheduler){
    bool last_node = false;
    if (scope.vertex() == ps.M+ps.N-1)
        last_node = true; 

    kcores_data & vdata = scope.vertex_data();
    if (!vdata.active){
       if (last_node)
          last_iter_kcores();
       return;
    }
    int cur_iter = ps.iiter + 1;
    if (vdata.degree <= cur_iter){
       vdata.active = false;
       vdata.kcore = cur_iter;
       vdata.degree = 0;
    } 
    else { 
      int incoming = 0;
      gl_types_kcores::edge_list outedgeid = scope.out_edge_ids();
      for(size_t i = 0; i < outedgeid.size(); ++i) {
        const kcores_data & other = scope.neighbor_vertex_data(scope.target(outedgeid[i]));
        if (other.active)
	  incoming++;
      }
      if (incoming <= cur_iter){
        vdata.active = false;
        vdata.kcore = cur_iter;
        vdata.degree = 0;
      }
    }


   if elast_node)
      last_iter_kcores();
};


void kcores_main(){

  ps.gt.start();
  calc_initial_degree();
 
  for (int i=0; i< ac.iter; i++){
   ps.glcore_kcores->start();
   ps.glcore_kcores->add_task_to_all(kcores_update_function, 1);
 }


}






