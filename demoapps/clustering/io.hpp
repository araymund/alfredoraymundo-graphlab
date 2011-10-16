/**  
 * Copyright (c) 2009 Carnegie Mellon University. 
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *  
 *  Code written by Danny Bickson, CMU
 *  Any changes to the code must include this original license notice in full.
 */


#ifndef _IO_HPP
#define _IO_HPP

#include "stats.hpp"
#include "../../libs/matrixmarket/mmio.h" //matrix market format support
#include <graphlab/macros_def.hpp>

extern advanced_config config;
extern problem_setup ps;
extern const char * inittypenames[];
void init();

template<typename edgedata>
int read_edges(FILE * f, int nodes, graph_type * _g);
template<typename edgedata>
int read_edges(FILE * f, int nodes, graph_type_kcores * _g);



void fill_output(graph_type * g){
  
   if (ac.algorithm == LDA)
	return;
  
   ps.output_clusters = zeros(ps.K, ps.N);
   for (int i=0; i<ps.K; i++)
     set_row(ps.output_clusters, i, ps.clusts.cluster_vec[i].location);
     
   int cols = 1;
   if (ac.algorithm == K_MEANS_FUZZY)
	cols = ac.K;
   ps.output_assignements = zeros(ps.M, cols);
     for (int i=0; i< ps.M; i++){ 
        const vertex_data & data = g->vertex_data(i);
        if (ac.algorithm == K_MEANS){
          set_val( ps.output_assignements, i,0, data.current_cluster);
        } 
	else if (ac.algorithm == K_MEANS_FUZZY){
           double factor = sum(pow(data.distances,-2));
           vec normalized = pow(data.distances,-2) / factor;
           set_row(ps.output_assignements, i, normalized);
        }
     }
} 
void fill_output(graph_type_kcores * g){
  
     ps.output_assignements = zeros(ps.M, 1);
     for (int i=0; i< ps.M; i++){ 
        const kcores_data & data = g->vertex_data(i);
        set_val(ps.output_assignements,i,0, data.kcore);
     }
} 




//write an output vector to file
void write_vec(FILE * f, int len, const double * array){
  assert(f != NULL && array != NULL);
  fwrite(array, len, sizeof(double), f);
}





//OUTPUT: SAVE output ot a binary file

// FORMAT:  M N K  (3 x ints, where M = number of data points, N = dimension of data point, K = number of clusters)
// MATRIX : output_clusters ( K x N doubles)
// MATRIX : output_assignments ( M x 1 doubles).
//          In case of FuzzyK-means or LDA, it is M x K doubles
// TOTAL FILE SIZE: 3 ints + (K*N + M*1) doubles
//                  3 ints + (M+N)*K doubles - for fuzzy kmeans/LDA
template<typename graph_type>
void export_to_binary_file(){

  fill_output(ps.g<graph_type>());

  char dfile[256] = {0};
  sprintf(dfile,"%s%d.out",ac.datafile.c_str(),ps.K);
  logstream(LOG_INFO)<<"Writing binary output file: " << dfile << std::endl;
  FILE * f = fopen(dfile, "w");
  assert(f!= NULL);

  int rc = fwrite(&ps.M, 1, 4, f);
  assert(rc == 4);
  rc = fwrite(&ps.N, 1, 4, f);
  assert(rc == 4);
  rc = fwrite(&ps.K, 1, 4, f);
  assert(rc == 4);
  write_vec(f, ps.output_clusters.rows() * ps.output_clusters.cols(), data(ps.output_clusters));
  write_vec(f, ps.output_assignements.rows() * ps.output_assignements.cols(), data(ps.output_assignements));
  fclose(f); 

}


//OUTPUT: SAVE FACTORS U,V,T TO IT++ FILE
template<typename graph_type>
void export_to_itpp_file(){

  fill_output(ps.g<graph_type>());

  char dfile[256] = {0};
  sprintf(dfile,"%s%d.out",ac.datafile.c_str(), ac.D);
  it_file output(dfile);
  output << Name("Clusters");
  output << ps.output_clusters;
  output << Name("Assignments");
  output << ps.output_assignements;
  output.close();
}


template<typename graph_type>
void export_to_matrixmarket(){
  fill_output(ps.g<graph_type>());
  
  char dfile[256] = {0};
  sprintf(dfile,"%s%d",ac.datafile.c_str(), ps.K);
  save_matrix_market_format(dfile);  
}

//LOAD FACTORS FROM FILE
void import_from_file(){

 mat clusters;
 vec assignments;
 char dfile[256] = {0};
 sprintf(dfile,"%s%d.out",ac.datafile.c_str(), ac.D);
 printf("Loading clusters from file\n");
 it_file input(dfile);
 input >> Name("Clusters");
 input  >> clusters;
 input >> Name("Assignments");
 input >> assignments;
 input.close();
 //saving output to file 
 for (int i=0; i< ps.M; i++){ 
    vertex_data & data = ps.g<graph_type>()->vertex_data(i);
    data.current_cluster = assignments[i]; 
 }
 for (int i=0; i<ps.K; i++){
    ps.clusts.cluster_vec[i].location = get_row(clusters,i);
 }
}

void add_vertices(graph_type * _g){
  assert(ps.K > 0);
  vertex_data vdata;
  // add M movie nodes (ps.tensor dim 1)
  for (int i=0; i<ps.M; i++){
    switch (ps.init_type){
       case INIT_RANDOM:
         vdata.current_cluster = randi(0, ps.K-1);
         break;
       
       case INIT_ROUND_ROBIN:
	 vdata.current_cluster = i % ps.K;
         break;

       case INIT_KMEANS_PLUS_PLUS: //is done later
       case INIT_RANDOM_CLUSTER:
	 vdata.current_cluster = -1;
	 break;
    }

    set_size(vdata.datapoint, ps.N);
    if (ps.algorithm == K_MEANS_FUZZY)
	vdata.distances = zeros(ps.K);

    _g->add_vertex(vdata);
    if (ac.debug && (i<= 5 || i == ps.M-1))
       std::cout<<"node " << i <<" initial assignment is: " << vdata.current_cluster << std::endl; 
 }
  
}


void add_vertices(graph_type_kcores * _g){
  assert(ps.K > 0);
  kcores_data vdata;
  for (int i=0; i<ps.M + ps.N; i++){
    _g->add_vertex(vdata);
 }
  
}


/* function that reads the problem from file */
/* Input format is:
 * M - number of matrix rows
 * N - number of matrix cols
 * zero - unused
 * e - number of edges (int)
 * A list of edges in the format
 * [from] [to] [weight]  (3 floats)
 * where [from] is an integeter from 1 to M
 * [to] is an interger from 1 to N
 * [weight] float
 */
template<typename graph_type>
void load_graph(const char* filename, graph_type * _g) {


  if (ac.matrixmarket){
      printf("Loading Matrix Market file %s\n", filename);
      load_matrix_market(filename, _g);
      return;
  }

  printf("Loading %s\n", filename);
  FILE * f = fopen(filename, "r");
  if(f== NULL){
	logstream(LOG_ERROR) << " can not find input file. aborting " << std::endl;
	exit(1);
  }

  int _M,_N,_K;
  int rc = fread(&_M,1,4,f);//matrix rows
  assert(rc==4); 
  rc=fread(&_N,1,4,f);//matrix cols
  assert(rc==4); 
  rc=fread(&_K,1,4,f);//unused
  assert(rc==4);
  if (!ac.supportgraphlabcf){
     if (_K != 0){
       logstream(LOG_ERROR) << "Error reading binary file: " << filename << " header. 3rd int value shoud be reserved to be zero. If you are using sparse matrix market format do not forget to call the program with --matrixmarket=true flag" << std::endl;
       exit(1);
     }  
   
  }
  else assert(_K >= 1);
  assert(_M>=1 && _N>=1); 
  


  ps.M=_M; ps.N= _N;
  init();
  add_vertices(_g);
 
  // read tensor non zero edges from file
  
 int val;
  if (!ac.supportgraphlabcf)
    val = read_edges<edge_float>(f,ps.N, _g);
  else {
      if (ac.FLOAT)
    val = read_edges<edge_float_cf>(f,ps.N,_g);
    else val = read_edges<edge_double_cf>(f,ps.N,_g);
  }
  assert(val == ps.L);

  fclose(f);
}
/**
 * read edges from file, with support with multiple edges between the same pair of nodes (in different times)
 */
template<typename edgedata>
int read_edges(FILE * f, int column_dim, graph_type_kcores * _g){
     
  int matlab_offset = 1; //matlab array start from 1

  unsigned int e;
  int rc = fread(&e,1,4,f);
  assert(rc == 4);
  printf("Creating %d edges (observed ratings)...\n", e);
  assert(e>0);

  int total = 0;
  edgedata* ed = new edgedata[200000];
  int edgecount_in_file = e;
  kcores_edge edge;
  while(true){
    rc = (int)fread(ed, sizeof(edgedata), std::min(200000, edgecount_in_file - total), f);
    total += rc;

    //go over each rating (edges)
    //#pragma omp parallel for
    for (int i=0; i<rc; i++){
      //verify node ids are in allowed range
      assert((int)ed[i].from >= matlab_offset && (int)ed[i].from <= ps.M);
      if (!ac.supportgraphlabcf)
        ed[i].to += ps.M;
      
      assert((int)ed[i].to > ps.M && (int)ed[i].to <= ps.N + ps.M);
      ed[i].from = ed[i].from - matlab_offset;
      ed[i].to = ed[i].to - matlab_offset;
      assert(ed[i].to != ed[i].from);
   }  
   
   for (int i=0; i<rc; i++){ 
      edge.weight = ed[i].weight;
      _g->add_edge(ed[i].from, ed[i].to, edge);
      //_g->add_edge(ed[i].to, ed[i].from, edge);
    }
      printf(".");
      fflush(0);
      if (rc == 0 || total >= edgecount_in_file)
       break;

  }
  if (total != (int)e){
      logstream(LOG_ERROR) << "Missing edges in file. Should be " << e << " but in file we counted only " << total << " edges. Please check your conversion script and verify the file is not truncated and edges are not missing. " << std::endl;
  }
  assert(total == (int)e);
  delete [] ed; ed = NULL;
  ps.L = e;
  return e;
}



/**
 * read edges from file, with support with multiple edges between the same pair of nodes (in different times)
 */
template<typename edgedata>
int read_edges(FILE * f, int column_dim, graph_type * _g){
     
  int matlab_offset = 1; //matlab array start from 1

  unsigned int e;
  int rc = fread(&e,1,4,f);
  assert(rc == 4);
  printf("Creating %d edges (observed ratings)...\n", e);
  assert(e>0);

  int total = 0;
  edgedata* ed = new edgedata[200000];
  int edgecount_in_file = e;
  while(true){
    rc = (int)fread(ed, sizeof(edgedata), std::min(200000, edgecount_in_file - total), f);
    total += rc;

    //go over each rating (edges)
    #pragma omp parallel for
    for (int i=0; i<rc; i++){
      if (!ac.zero) //usually we do not allow zero entries, unless --zero=true flag is set.
	 assert(ed[i].weight != 0); 
      //verify node ids are in allowed range
      assert((int)ed[i].from >= matlab_offset && (int)ed[i].from <= ps.M);
      if (ac.supportgraphlabcf)
        ed[i].to -= ps.M;
      assert((int)ed[i].to >= matlab_offset && (int)ed[i].to <= ps.N);
      //no self edges
      //assert((int)ed[i].to != (int)ed[i].from);
    
      //if sacling of rating values is requested to it here.
      if (ac.scalerating != 1.0)
	     ed[i].weight /= ac.scalerating;
   }  
   
   for (int i=0; i<rc; i++){ 
     vertex_data & vdata = _g->vertex_data(ed[i].from - matlab_offset);
      set_new( vdata.datapoint, ed[i].to - matlab_offset, ed[i].weight);  
      if (ac.algorithm == K_MEANS){ //compute mean for each cluster by summing assigned points
         ps.clusts.cluster_vec[vdata.current_cluster].cur_sum_of_points[ed[i].to - matlab_offset] += ed[i].weight;  
      }
      if (! vdata.reported){
         vdata.reported = true;
         if (ac.algorithm == K_MEANS) 
           ps.clusts.cluster_vec[vdata.current_cluster].num_assigned_points++;
         ps.total_assigned++; //count the total number of non-zero rows we encountered
       }
    }
      printf(".");
      fflush(0);
      if (rc == 0 || total >= edgecount_in_file)
       break;

  }
  if (total != (int)e){
      logstream(LOG_ERROR) << "Missing edges in file. Should be " << e << " but in file we counted only " << total << " edges. Please check your conversion script and verify the file is not truncated and edges are not missing. " << std::endl;
  }
  assert(total == (int)e);
  delete [] ed; ed = NULL;
  ps.L = e;
  return e;
}



#include <graphlab/macros_undef.hpp>
#endif
