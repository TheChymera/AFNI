#include "SUMA_suma.h"

#undef STAND_ALONE

#if defined SUMA_SurfSmooth_STAND_ALONE
#define STAND_ALONE 
#endif

#ifdef STAND_ALONE
/* these global variables must be declared even if they will not be used by this main */
SUMA_SurfaceViewer *SUMAg_cSV = NULL; /*!< Global pointer to current Surface Viewer structure*/
SUMA_SurfaceViewer *SUMAg_SVv = NULL; /*!< Global pointer to the vector containing the various Surface Viewer Structures */
int SUMAg_N_SVv = 0; /*!< Number of SVs stored in SVv */
SUMA_DO *SUMAg_DOv = NULL;   /*!< Global pointer to Displayable Object structure vector*/
int SUMAg_N_DOv = 0; /*!< Number of DOs stored in DOv */
SUMA_CommonFields *SUMAg_CF = NULL; /*!< Global pointer to structure containing info common to all viewers */
#else
extern SUMA_CommonFields *SUMAg_CF;
extern SUMA_DO *SUMAg_DOv;
extern SUMA_SurfaceViewer *SUMAg_SVv;
extern int SUMAg_N_SVv; 
extern int SUMAg_N_DOv;  
#endif

/*!

   \brief A function to calculate the geodesic distance of nodes connected to node n
          See labbook NIH-3 pp 138 and on for notes on algorithm 
   \param n (int) index of center node
   \param SO (SUMA_SurfaceObject *) structure containing surface object
   \param off (float *) a vector such that off[i] = the geodesic distance of node i
                        to node n. The vector should be initialized to -1.0
   \param lim (float) maximum geodesic distance to travel
   
   - This function is too slow. See SUMA_getoffsets2
   \sa SUMA_getoffsets2
*/
#define DBG 1
#define DoCheck 1
SUMA_Boolean SUMA_getoffsets (int n, SUMA_SurfaceObject *SO, float *Off, float lim) 
{
   static char FuncName[]={"SUMA_getoffsets"};
   int i, ni, iseg;
   float Off_tmp;
   SUMA_Boolean Visit = NOPE;
   static SUMA_Boolean LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   #if DoCheck
   if (!SO->FN || !SO->EL) {
      SUMA_SL_Err("SO->FN &/| SO->EL are NULL.\n");
      SUMA_RETURN(NOPE);
   }
   #endif
   
   #if DBG
   if (LocalHead) fprintf(SUMA_STDERR,"%s: Working node %d, %d neighbs. lim = %f\n", 
                                    FuncName, n, SO->FN->N_Neighb[n], lim);
   #endif
   
   for (i=0; i < SO->FN->N_Neighb[n]; ++i) {
      ni = SO->FN->FirstNeighb[n][i]; /* for notational sanity */
      iseg = SUMA_FindEdge (SO->EL, n, SO->FN->FirstNeighb[n][i]);
      #if DoCheck
      if (iseg < 0) {
         SUMA_SL_Err("Failed to find segment");
         SUMA_RETURN(NOPE);
      }
      #endif
      
      Off_tmp = Off[n] + SO->EL->Le[iseg];   /* that is the distance from n (original n) to ni along
                                                that particular path */
                                             
      Visit = NOPE;
      if (Off[ni] < 0 || Off_tmp < Off[ni]) { /* Distance improvement, visit/revist that node */
         if (Off_tmp < lim) { /* only record if less than lim */
            Visit = YUP;
            Off[ni] = Off_tmp;
         } 
      } 
      
      #if DBG
      if (LocalHead) fprintf(SUMA_STDERR,"%s: %d --> %d. Visit %d, Current %f, Old %f\n", 
         FuncName, n, ni, Visit, Off_tmp, Off[ni]);
      #endif
      
      #if 0
         { int jnk; fprintf(SUMA_STDOUT,"Pausing ..."); jnk = getchar(); fprintf(SUMA_STDOUT,"\n"); }
      #endif

      if (Visit) { /* a new node has been reached with an offset less than limit, go down that road */
         if (!SUMA_getoffsets (ni, SO, Off, lim))  {
            SUMA_SL_Err("Failed in SUMA_getoffsets");
            SUMA_RETURN (NOPE);
         }
      }
   }

   SUMA_RETURN(YUP);
}

/*!
   \brief Allocate and initialize SUMA_GET_OFFSET_STRUCT* struct 
   OffS = SUMA_Initialize_getoffsets (N_Node);
   
   \param N_Node(int) number of nodes forming mesh 
   \return OffS (SUMA_GET_OFFSET_STRUCT *) allocate structure
           with initialized fields for zeroth order layer
   
   \sa SUMA_AddNodeToLayer
   \sa SUMA_Free_getoffsets
   \sa SUMA_Initialize_getoffsets
*/           
   
SUMA_GET_OFFSET_STRUCT *SUMA_Initialize_getoffsets (int N_Node)
{
   static char FuncName[]={"SUMA_Initialize_getoffsets"};
   int i;
   SUMA_GET_OFFSET_STRUCT *OffS = NULL;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   if (N_Node <= 0) {
      SUMA_SL_Err("Bad values for N_Node");
      SUMA_RETURN (OffS);
   }
   
   OffS = (SUMA_GET_OFFSET_STRUCT *)SUMA_malloc(sizeof(SUMA_GET_OFFSET_STRUCT));
   if (!OffS) {
      SUMA_SL_Err("Failed to allocate for OffS");
      SUMA_RETURN (OffS);
   }
   
   OffS->OffVect = (float *) SUMA_malloc(N_Node * sizeof(float));
   OffS->LayerVect = (int *) SUMA_malloc(N_Node * sizeof(int));
   OffS->N_Nodes = N_Node;
   
   if (!OffS->LayerVect || !OffS->OffVect) {
      SUMA_SL_Err("Failed to allocate for OffS->LayerVect &/| OffS->OffVect");
      SUMA_free(OffS);
      SUMA_RETURN (OffS);
   }
   
   /* initialize vectors */
   for (i=0; i< N_Node; ++i) {
      OffS->OffVect[i] = 0.0;
      OffS->LayerVect[i] = -1;
   }
   
   /* add a zeroth layer for node n */
   OffS->N_layers = 1;
   OffS->layers = (SUMA_NODE_NEIGHB_LAYER *) SUMA_malloc(OffS->N_layers * sizeof(SUMA_NODE_NEIGHB_LAYER));
   OffS->layers[0].N_AllocNodesInLayer = 1;
   OffS->layers[0].NodesInLayer = (int *) SUMA_malloc(OffS->layers[0].N_AllocNodesInLayer * sizeof(int));
   OffS->layers[0].N_NodesInLayer = 0;   
   
   SUMA_RETURN (OffS);
   
}

/*!
   \brief Add node n to neighboring layer LayInd in OffS
   ans = SUMA_AddNodeToLayer (n, LayInd, OffS);
   
   \param n (int)
   \param LayInd (int)
   \param OffS (SUMA_GET_OFFSET_STRUCT *)
   \return YUP/NOPE (good/bad)
   
   - allocation is automatically taken care of
   
   \sa SUMA_Free_getoffsets
   \sa SUMA_Initialize_getoffsets
*/
SUMA_Boolean SUMA_AddNodeToLayer (int n, int LayInd, SUMA_GET_OFFSET_STRUCT *OffS)
{
   static char FuncName[]={"SUMA_AddNodeToLayer"};
   static SUMA_Boolean LocalHead = NOPE;
   
   /* is this a new layer */
   if (LayInd > OffS->N_layers) { /* error */
      SUMA_SL_Err("LayInd > OffS->N_layers. This should not be!");
      SUMA_RETURN(NOPE);
   } else if (LayInd == OffS->N_layers) { /* need a new one */
      SUMA_LH("Adding layer");
      OffS->N_layers += 1;
      OffS->layers = (SUMA_NODE_NEIGHB_LAYER *) SUMA_realloc(OffS->layers, OffS->N_layers*sizeof(SUMA_NODE_NEIGHB_LAYER));
      OffS->layers[LayInd].N_AllocNodesInLayer = 200;
      OffS->layers[LayInd].NodesInLayer = (int *) SUMA_malloc(OffS->layers[LayInd].N_AllocNodesInLayer * sizeof(int));
      OffS->layers[LayInd].N_NodesInLayer = 0;
   }
   
   OffS->layers[LayInd].N_NodesInLayer += 1;
   /* do we need to reallocate for NodesInLayer ? */
   if (OffS->layers[LayInd].N_NodesInLayer ==  OffS->layers[LayInd].N_AllocNodesInLayer) { /* need more space */
      SUMA_LH("reallocating neighbors");
      OffS->layers[LayInd].N_AllocNodesInLayer += 200;
      OffS->layers[LayInd].NodesInLayer = (int *) SUMA_realloc (OffS->layers[LayInd].NodesInLayer, OffS->layers[LayInd].N_AllocNodesInLayer * sizeof(int));
   }
   
   OffS->layers[LayInd].NodesInLayer[OffS->layers[LayInd].N_NodesInLayer - 1] = n;
   
   SUMA_RETURN(YUP); 
}

/*!
   \brief free memory associated with SUMA_GET_OFFSET_STRUCT * struct
   
   \param OffS (SUMA_GET_OFFSET_STRUCT *) Offset strcture
   \return NULL
   
   \sa SUMA_Recycle_getoffsets
   \sa SUMA_Initialize_getoffsets
*/
SUMA_GET_OFFSET_STRUCT * SUMA_Free_getoffsets (SUMA_GET_OFFSET_STRUCT *OffS) 
{
   static char FuncName[]={"SUMA_Free_getoffsets"};
   int i = 0;
   static SUMA_Boolean LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   if (!OffS) SUMA_RETURN(NULL);
   
   if (OffS->layers) {
      for (i=0; i< OffS->N_layers; ++i) if (OffS->layers[i].NodesInLayer) SUMA_free(OffS->layers[i].NodesInLayer);
      SUMA_free(OffS->layers);
   }
   
   if (OffS->OffVect) SUMA_free(OffS->OffVect);
   if (OffS->LayerVect) SUMA_free(OffS->LayerVect);
   SUMA_free(OffS); OffS = NULL;
   
   SUMA_RETURN(NULL);
}

/*!
   \brief reset the SUMA_GET_OFFSET_STRUCT after it has been used by a node
   \param OffS (SUMA_GET_OFFSET_STRUCT *) Offset structure that has node neighbor
                                          info and detail to be cleared
   \return (YUP/NOPE) success/failure 
   
   - No memory is freed here
   - The used node layer indices are reset to -1
   - The number of nodes in each layer are reset to 0
   
   \sa SUMA_Free_getoffsets to free this structure once and for all
   \sa SUMA_Initialize_getoffsets
*/
SUMA_Boolean SUMA_Recycle_getoffsets (SUMA_GET_OFFSET_STRUCT *OffS)
{
   static char FuncName[]={"SUMA_Recycle_getoffsets"};
   int i, j;
   static SUMA_Boolean LocalHead = NOPE;
   
   for (i=0; i < OffS->N_layers; ++i) {
      /* reset the layer index of used nodes in LayerVect */
      for (j=0; j < OffS->layers[i].N_NodesInLayer; ++j) {
         OffS->LayerVect[OffS->layers[i].NodesInLayer[j]] = -1;
      }
      /* reset number of nodes in each layer */
      OffS->layers[i].N_NodesInLayer = 0;
   }
   
   SUMA_RETURN(YUP);
}

/*!
   \brief calculates the length of the segments defined
   by a node and its first-order neighbors. The resulting
   matrix very closely resembles SO->FN->FirstNeighb
   DistFirstNeighb = SUMA_CalcNeighbDist (SO);
   
   \param SO (SUMA_SurfaceObject *) with FN field required
   \return DistFirstNeighb (float **) DistFirstNeighb[i][j] contains the 
                                      length of the segment formed by nodes
                                      SO->FN->NodeId[i] and SO->FN->FirstNeighb[i][j]
                                      
   This function was created to try and speed up SUMA_getoffsets2 but it proved
   useless.
   Sample code showing two ways of getting segment length:
   #if 1
         // calculate segment distances(a necessary horror) 
         // this made no difference in speed  
         DistFirstNeighb = SUMA_CalcNeighbDist (SO);
         if (!DistFirstNeighb) { 
            SUMA_SL_Crit("Failed to allocate for DistFirstNeighb\n");
            exit(1);
         }
         { int n1, n2, iseg;
            n1 = 5; n2 = SO->FN->FirstNeighb[n1][2];
            iseg = SUMA_FindEdge(SO->EL, n1, n2);
            fprintf(SUMA_STDERR, "%s: Distance between nodes %d and %d:\n"
                                 "from DistFirstNeighb = %f\n"
                                 "from SO->EL->Le = %f\n", FuncName, n1, n2,
                                 DistFirstNeighb[n1][2], SO->EL->Le[iseg]);
            exit(1);
         } 
   #endif
*/   
   
float ** SUMA_CalcNeighbDist (SUMA_SurfaceObject *SO) 
{
   static char FuncName[]={"SUMA_CalcNeighbDist"};
   float **DistFirstNeighb=NULL, *a, *b;
   int i, j;
   static SUMA_Boolean LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   if (!SO) { SUMA_RETURN(NULL); }
   if (!SO->FN) { SUMA_RETURN(NULL); }
   
   DistFirstNeighb = (float **)SUMA_allocate2D(SO->FN->N_Node, SO->FN->N_Neighb_max, sizeof(float));
   if (!DistFirstNeighb) {
      SUMA_SL_Crit("Failed to allocate for DistFirstNeighb");
      SUMA_RETURN(NULL);
   }
   for (i=0; i < SO->FN->N_Node; ++i) {
      a = &(SO->NodeList[3*SO->FN->NodeId[i]]);
      for (j=0; j < SO->FN->N_Neighb[i]; ++j) {
         b = &(SO->NodeList[3*SO->FN->FirstNeighb[i][j]]);
         SUMA_SEG_LENGTH(a, b, DistFirstNeighb[i][j]);
         if (SO->FN->NodeId[i] == 5 && SO->FN->FirstNeighb[i][j] == 133092) {
            fprintf (SUMA_STDERR, "%f %f %f\n%f %f %f\n%f\n", 
               SO->NodeList[3*SO->FN->NodeId[i]], SO->NodeList[3*SO->FN->NodeId[i]+1], SO->NodeList[3*SO->FN->NodeId[i]+2],
               SO->NodeList[3*SO->FN->FirstNeighb[i][j]], SO->NodeList[3*SO->FN->FirstNeighb[i][j]+1], 
               SO->NodeList[3*SO->FN->FirstNeighb[i][j]+2], DistFirstNeighb[i][j]);
         }
      }
   }
   
   SUMA_RETURN (DistFirstNeighb);
}

/*!
   \brief A function to calculate the geodesic distance of nodes connected to node n
           SUMA_getoffsets was the first incarnation but it was too slow.
    ans = SUMA_getoffsets2 (n, SO, lim, OffS) 
   
   \param n (int) index of center node
   \param SO (SUMA_SurfaceObject *) structure containing surface object
   \param lim (float) maximum geodesic distance to travel
   \param OffS (SUMA_GET_OFFSET_STRUCT *) initialized structure to contain
          the nodes that neighbor n within lim mm 
   \return ans (SUMA_Boolean) YUP = GOOD, NOPE = BAD
   
   \sa SUMA_AddNodeToLayer
   \sa SUMA_Free_getoffsets
   \sa SUMA_Initialize_getoffsets



The following code was used to test different methods for calculating the segment length,
none (except for Seg = constant) proved to be faster, probably because of memory access time.
One of the options required the use of DistFirstNeighb which is calculated by function
SUMA_CalcNeighbDist. It mirrors SO->EL->FirstNeighb  

static int SEG_METHOD;
switch (SEG_METHOD) {
   case CALC_SEG: 
      // this is the slow part, too many redundant computations. 
      //cuts computation time by a factor > 3 if Seg was set to a constant
      //However, attempts at accessing pre-calculated segment lengths
      //proved to be slower. 
      SUMA_SEG_LENGTH (a, b, Seg); 
      break;
   case FIND_EDGE_MACRO:
      // this one's even slower, calculations have been made once but
      //function calls are costly (7.53 min)
      iseg = -1;
      if (n_k < n_jne) {SUMA_FIND_EDGE (SO->EL, n_k, n_jne, iseg);}
      else {SUMA_FIND_EDGE (SO->EL, n_jne, n_k, iseg);}
      if (iseg < 0) { 
         SUMA_SL_Err("Segment not found.\nSetting Seg = 10000.0");
         Seg = 10000.0; 
      } else Seg = SO->EL->Le[iseg];
      break;
   case FIND_EDGE:
      //this one's even slower, calculations have been made once but
      //function calls are costly
      iseg = SUMA_FindEdge (SO->EL, n_k, n_jne); 
      Seg = SO->EL->Le[iseg];
      break;

   case DIST_FIRST_NEIGHB:
      // consumes memory but might be faster than previous 2 (5.22 min)
      Seg = DistFirstNeighb[n_jne][k];
      break;
   case CONST:
      // 1.7 min 
      Seg = 1.0;
      break;
   default:
      SUMA_SL_Err("Bad option");
      break;
}                    
*/
SUMA_Boolean SUMA_getoffsets2 (int n, SUMA_SurfaceObject *SO, float lim, SUMA_GET_OFFSET_STRUCT *OffS) 
{
   static char FuncName[]={"SUMA_getoffsets"};
   int LayInd, il, n_il, n_jne, k, n_prec = -1, n_k, jne, iseg=0;
   float Off_tmp, Seg, *a, *b, minSeg;
   SUMA_Boolean Visit = NOPE;
   SUMA_Boolean AllDone = NOPE;
   static SUMA_Boolean LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   if (!OffS) {
      SUMA_SL_Err("NULL OffS");
      SUMA_RETURN(NOPE);
   }
   
   /* setup 0th layer */
   OffS->OffVect[n] = 0.0;   /* n is at a distance 0.0 from itself */
   OffS->LayerVect[n] = 0;   /* n is on the zeroth layer */
   OffS->layers[0].N_NodesInLayer = 1;
   OffS->layers[0].NodesInLayer[0] = n;
   
   LayInd = 1;  /* index of next layer to build */
   AllDone = NOPE;
   while (!AllDone) {
      
      AllDone = YUP; /* assume that this would be the last layer */
      for (il=0; il < OffS->layers[LayInd - 1].N_NodesInLayer; ++il) { /* go over all nodes in previous layer */
         n_il =  OffS->layers[LayInd - 1].NodesInLayer[il]; /* node from previous layer */
         
         for (jne=0; jne < SO->FN->N_Neighb[n_il]; ++jne) { /* go over all the neighbours of node n_il */
            n_jne = SO->FN->FirstNeighb[n_il][jne];        /* node that is an immediate neighbor to n_il */
            
            if (OffS->LayerVect[n_jne] < 0) { /* node is not assigned to a layer yet */
               OffS->LayerVect[n_jne] =  LayInd;    /* assign new layer index to node */
               OffS->OffVect[n_jne] = 0.0;          /* reset its distance from node n */
               SUMA_AddNodeToLayer (n_jne, LayInd, OffS);   /* add the node to the nodes in the layer */
               minSeg = 100000.0; n_prec = -1; Seg = 0.0;
               for (k=0; k < SO->FN->N_Neighb[n_jne]; ++k) { /* calculate shortest distance of node to any precursor */  
                  n_k = SO->FN->FirstNeighb[n_jne][k];
                  if (OffS->LayerVect[n_k] == LayInd - 1) { /* this neighbor is a part of the previous layer, good */
                     a = &(SO->NodeList[3*n_k]); b = &(SO->NodeList[3*n_jne]);
                     /* this is the slow part, too many redundant computations. 
                        Computation time is cut by a factor > 2 if Seg was set to a constant
                        However, attempts at accessing pre-calculated segment lengths
                        proved to be slower. See Comments in function help*/
                     SUMA_SEG_LENGTH_SQ (a, b, Seg);                    
                     if (Seg < minSeg) {
                        minSeg = Seg;
                        n_prec = n_k;
                     }
                  }
               }/* for k */
               
               if (n_prec < 0) { /* bad news */
                  SUMA_SL_Crit("No precursor found for node.");
                  OffS = SUMA_Free_getoffsets (OffS);
                  SUMA_RETURN(NOPE);
               } else {
                  OffS->OffVect[n_jne] = OffS->OffVect[n_prec] + sqrt(Seg);
                  if (OffS->OffVect[n_jne] < lim) { /* must go at least one more layer */
                     AllDone = NOPE;
                  }
               }
            } /* node not already in layer */
            
         } /* for jne */
      
      } /* for il */    
      if (LocalHead) fprintf (SUMA_STDERR,"%s: On to layer %d\n", FuncName, LayInd);
      ++LayInd;
   } /* while AllDone */
   
   SUMA_RETURN(YUP);
}

/*!
   \brief calculate the interpolation weights required to smooth data on the surface
   using M.K. Chung et al. Neuroimage 03
   
   \param SO (SUMA_SurfaceObject *) Surface object with valid SO->NodeList, SO->FaceSetList and SO->FN
   \return wgt (float **) 2D matrix of the same size as SO->FirstNeighb that contains the
                           weights to be applied to a node's neighbors in interpolation
                           Free the result with SUMA_free2D ((char **)wgt, SO->N_Node);
                           The weights are computed using the Finite Element method.
   \sa SUMA_Chung_Smooth
*/
float ** SUMA_Chung_Smooth_Weights (SUMA_SurfaceObject *SO)
{
   static char FuncName[]={"SUMA_Chung_Smooth_Weights"};
   float **wgt=NULL, *coord_nbr=NULL, *cotan=NULL, *tfp=NULL;
   float dv[3], p[3], q[3];
   float area, area_p, area_q, dot_p, dot_q;
   int i, j, k, n, j3p1, j3m1, n3, j3=0, nj, nj3, i_dbg;
   SUMA_Boolean LocalHead = YUP;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   if (!SO) {
      SUMA_SL_Err("Null SO");
      SUMA_RETURN(NULL);
   }
   if (!SO->FN) {
      SUMA_SL_Err("Null SO->FN");
      SUMA_RETURN(NULL);
   }
   
   i_dbg = 17; /* index of debugging node, set to -1 for no debugging */
   
   /* implement the non-parametric weight estimation method */
   wgt = (float **)SUMA_allocate2D(SO->N_Node, SO->FN->N_Neighb_max, sizeof(float));  /* vector of node weights */
   coord_nbr = (float *)SUMA_malloc((SO->FN->N_Neighb_max + 2) * sizeof(float) * 3); /* vector of neighboring node coordinates */ 
   cotan = (float *)SUMA_malloc(SO->FN->N_Neighb_max * sizeof(float)); 
   if (!wgt || !coord_nbr || !cotan) {
      SUMA_SL_Crit("Failed to allocate for wgt &/|coord_nbr &/|cotan");
      SUMA_RETURN(NULL);
   }
   
   for (n=0; n < SO->N_Node; ++n) {
      n3 = 3 * n;
      /* translate the coordinates of the neighboring nodes to make n be the origin */
      for (j=0; j<SO->FN->N_Neighb[n]; ++j) {
         j3 = 3 * (j+1);
         nj = SO->FN->FirstNeighb[n][j]; nj3 = 3 * nj;
         coord_nbr[j3] = SO->NodeList[nj3] - SO->NodeList[n3];
         coord_nbr[j3+1] = SO->NodeList[nj3+1] - SO->NodeList[n3+1];
         coord_nbr[j3+2] = SO->NodeList[nj3+2] - SO->NodeList[n3+2];
      }  /* for j */
      /* padd with last neighbor at the very beginning and 1st neighbor at the end 
         in matlab: coord_nbr = [coord_nbr(:,last_neighb) coord_nbr coord_nbr(:,first_neighb)];
      */   
      for (k=0; k < 3; ++k) coord_nbr[k] = coord_nbr[j3+k];  
      j3 = 3 * ( SO->FN->N_Neighb[n] + 1);
      for (k=0; k < 3; ++k) coord_nbr[j3+k] = coord_nbr[3+k];
      if (LocalHead && n == i_dbg) { SUMA_disp_vect (coord_nbr, 3 * (SO->FN->N_Neighb[n] + 2)) ;  }
      
      
      area = 0.0;
      for (j=1; j<=SO->FN->N_Neighb[n]; ++j) { 
         j3 = 3 * j; j3p1 = 3 * (j+1); j3m1 = 3 * (j-1);
         for (k=0; k < 3; ++k) dv[k] = coord_nbr[j3p1+k] - coord_nbr[j3+k]; 
         tfp = &(coord_nbr[j3p1]);
         dot_p = SUMA_MT_DOT (tfp, dv);
         SUMA_MT_CROSS(p, tfp, dv);
         for (k=0; k < 3; ++k) dv[k] = coord_nbr[j3m1+k] - coord_nbr[j3+k]; 
         tfp = &(coord_nbr[j3m1]);
         dot_q = SUMA_MT_DOT (tfp, dv);
         SUMA_MT_CROSS(q, tfp, dv);
         
         SUMA_NORM(area_p, p); 
         SUMA_NORM(area_q, q); 
         
         cotan[j-1] = dot_p/area_p + dot_q/area_q;
         area += area_p/2.0;
         if (LocalHead && n == i_dbg) {
            fprintf (SUMA_STDERR,"[%d->%d] area_p, area_q = %f, %f\n",
                                  n, SO->FN->FirstNeighb[n][j-1],
                                  area_p / 2.0,  area_q / 2.0);
         }
      }
      
      for (j=0; j<SO->FN->N_Neighb[n]; ++j) {
         wgt[n][j] = cotan[j]/area;
      }
      if (LocalHead && n == i_dbg) {
         fprintf (SUMA_STDERR,"%s: Weight Results for neighbors of %d (matlab node %d):\n",
                              FuncName, n, n+1);
         SUMA_disp_vect (wgt[n], SO->FN->N_Neighb[n]);
      }
   }  /* for n */

   /* free local variables */
   if (coord_nbr) SUMA_free(coord_nbr); coord_nbr = NULL;
   if (cotan) SUMA_free(cotan); cotan = NULL;

   SUMA_RETURN(wgt);
}

/*!
   \brief Show the transfer function (f(k)) for the Taubin 
   smoothing algorithm for a combination of scaling factors
   and number of iterations 
   
   \param l (float)  postive scaling factor
   \param m (float)  negative scaling factor
                    (for avoiding the shrinkage of surfaces)
            The band-pass frequency (kbp) is 1/m + 1/l
            f(kbp) = 1
   \param N_iter (int) number of iterations
   \param Out (FILE *) pointer to output file. If NULL,
                  output is to stdout
   \return ans (SUMA_Boolean) YUP, no problems
                              NOPE, yes problems
   
   The output is of the form:
   k f(k)
   
   where k is the normalized frequency and 
   f(k) is the value of the transfer function at k
   
   \sa figure 4 in Geometric Signal Processing on 
   Polygonal Meshes (Taubin G, Eurographics 2000)
   \sa SUMA_Taubin_Smooth
   \sa SUMA_Taubin_Smooth_Coef
*/   
SUMA_Boolean  SUMA_Taubin_Smooth_TransferFunc (float l, float m, int N, FILE *Out)
{
   static char FuncName[]={"SUMA_Taubin_Smooth_TransferFunc"};
   FILE *Outp = NULL;
   int i, imax = 100;
   float fk, k;
   SUMA_Boolean LocalHead = YUP;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   if (N % 2) {
      SUMA_SL_Err("N_iter must be even");
      SUMA_RETURN(NOPE);
   }
   
   if (!Out) Outp = stdout;
   else Outp = Out;
   
   k = 0.0;
   for (i=0; i< imax; ++i) {
      fk = pow( ( ( 1-m*k ) * ( 1-l*k ) ) , N / 2 );
      fprintf (Outp,"%f %f\n", k, fk);
      k += (float)i/(float)imax;
   }
   
   
   SUMA_RETURN(YUP);
}
/*!
   \brief Calculates Mu(m) and Lambda(l) smoothing coefficients
   based on Taubin's smoothing algorithm in Geometric Signal 
   Processing on Polygonal Meshes (Eurographics 2000)
   
   \param k (float) the pass-band frequency (typically 0.1)
   \param *l (float) what will be the postive scaling factor
   \param *m (float) what will be the negative scaling factor
                     (for avoiding the shrinkage of surfaces)
   \return ans (SUMA_Boolean) YUP, good solution found
                              NOPE, solution could not be found        
            
   k, l and m are related by the following equations:
   
   k = 1/l + 1/m                 (eq 8)
   0 = 1 - 3(l+m) + 5lm          (eq 9)
   
   the solutions must verify the following:
   l > 0
   m < -l < 0
   
   \sa SUMA_Taubin_Smooth_TransferFunc 
   \sa SUMA_Taubin_Smooth
    
*/ 
SUMA_Boolean SUMA_Taubin_Smooth_Coef (float k, float *l, float *m)
{
   static char FuncName[]={"SUMA_Taubin_Smooth_Coef"};
   int i;
   float ls[2], delta;
   SUMA_Boolean Done = NOPE;
   SUMA_Boolean LocalHead = YUP;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   if (k < 0) { SUMA_SL_Err("k < 0"); SUMA_RETURN(NOPE); }
   
   /* l1 and l2 are solutions of the quadratic equation:
      (5 - 3 k) l^2 + k l - 1 = 0 */
   delta = ( k * k - 12.0 * k + 20 );
   if (delta < 0) { SUMA_SL_Err("Delta is < 0 for specified k"); SUMA_RETURN(NOPE); }
   
   ls[0] = ( -k + sqrt(delta) ) / ( 10 - 6 * k );
   ls[1] = ( -k - sqrt(delta) ) / ( 10 - 6 * k );
   if (ls[0] < 0 && ls[1] < 0) { SUMA_SL_Err("No positive solution for l"); SUMA_RETURN(NOPE); }
   
   if (ls[1] > ls[0]) { /* swap them */
      *l = ls[0]; ls[0] = ls[1]; ls[1] = *l;
   }
   
   Done = NOPE;
   i = 0;
   while (!Done && i < 2) {
      /* calculate mu */
      *l = ls[i]; 
      *m = *l / ( k * *l - 1.0 );
      if (*m < 0) Done = YUP;
      ++i;
   }
   
   if (!Done) { SUMA_SL_Err("No good solutions found."); SUMA_RETURN(NOPE); }
   
   if ( ! ( ( *m < -1.0 * *l ) && ( -1.0 * *l < 0 ) ) ) {
      SUMA_SL_Err("Solution did not meet m < -l < 0"); SUMA_RETURN(NOPE);
   }
   
   SUMA_RETURN(YUP);
}

/*!
   \brief performs smoothing based on Taubin's smoothing 
   algorithm in Geometric Signal Processing on Polygonal 
   Meshes (Eurographics 2000)
   
   fout =  SUMA_Taubin_Smooth (SUMA_SurfaceObject *SO, float **wgt, 
                            float lambda, float mu, float *fin, 
                            int N_iter, int vpn,  d_order,
                            float *fout_user);
                            
   \param SO (SUMA_SurfaceObject *SO) The surface, with NodeList, FaceSetList
                                       and FN fields present
   \param wgt (float **) interpolation weights for each node. 
                         The dimentions of wgt are equal to those of 
                         SO->FN->FirstNeighb
                         These weights may need to be re-evaluated for
                         each iteration
                         For equal weights (1/SO->FN->N_FirstNeighb[n]), 
                         just pass NULL         
   \param lambda (float) postive scaling factor
   \param mu (float) negative scaling factor 
   \param fin (float *) vector containing node data. The length of this vector
                        is vpn x SO->N_Node , where vpn is the number of values
                        per node. 
   \param N_iter (int)  number of iterations (same weights are used in each iteration)
   \param vpn (int) number of values per node in fin
   \param d_order (SUMA_INDEXING_ORDER) Indicates how multiple values per node are stored in fin
                        SUMA_ROW_MAJOR: The data in fin is stored in *** Row Major *** order.
                        The ith value (start at 0) for node n is at index fin[vpn*n+i]
                        SUMA_COLUMN_MAJOR: The data in fin is stored in *** Column Major *** order.
                        The ith (start at 0) value for node n is at index fin[n+SO->N_Node*i]; 
                        etc...
   \param fout_user (float *) a pointer to the vector where the smoothed version of fin will reside
                        You can pass NULL and the function will do the allocation for you and return 
                        the pointer to the smoothed data in fout.
                        If you already have space allocated for the result, then pass the pointer
                        in fout_user and save on allocation time and space. In that case, fout
                        is equal to fout_user.
                        Either way, you are responsible for freeing memory pointed to by fout.
                        DO NOT PASS fout_user = fin 
   \return fout (float *) A pointer to the smoothed data (vpn * SO->N_Node values). 
                        You will have to free the memory allocated for fout yourself.
                        
    
   
*/
float * SUMA_Taubin_Smooth (SUMA_SurfaceObject *SO, float **wgt, 
                            float lambda, float mu, float *fin_orig, 
                            int N_iter, int vpn, SUMA_INDEXING_ORDER d_order,
                            float *fout_final_user)
{
   static char FuncName[]={"SUMA_Taubin_Smooth"};
   float *fout_final=NULL, *fbuf=NULL, *fin=NULL, *fout=NULL, *fin_next=NULL;
   float fp, dfp, fpj;
   int n , k, j, niter, vnk, n_offset; 
   SUMA_Boolean LocalHead = YUP;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   if (N_iter % 2) {
      SUMA_SL_Err("N_iter must be an even number\n");
      SUMA_RETURN(NULL);
   }
   
   if (!SO || !fin_orig) {
      SUMA_SL_Err("NULL SO or fin_orig\n");
      SUMA_RETURN(NULL);
   }
   if (!SO->FN) {
      SUMA_SL_Err("NULL SO->FN\n");
      SUMA_RETURN(NULL);
   }
   
   if (vpn < 1) {
      SUMA_SL_Err("vpn < 1\n");
      SUMA_RETURN(NULL);
   }  
   
   if (fout_final_user == fin_orig) {
      SUMA_SL_Err("fout_final_user == fin_orig");
      SUMA_RETURN(NULL);
   }
   
   if (!fout_final_user) { /* allocate for output */
      fout_final = (float *)SUMA_calloc(SO->N_Node * vpn, sizeof(float));
      if (!fout_final) {
         SUMA_SL_Crit("Failed to allocate for fout_final\n");
         SUMA_RETURN(NULL);
      }
   }else {
      fout_final = fout_final_user; /* pre-allocated */
   }
   
   /* allocate for buffer */
   fbuf = (float *)SUMA_calloc(SO->N_Node * vpn, sizeof(float));
   if (!fbuf) {
      SUMA_SL_Crit("Failed to allocate for fbuf\n");
      SUMA_RETURN(NULL);
   }
   
   if (LocalHead) {
      fprintf (SUMA_STDERR,"%s: Mu = %f, Lambda = %f\nShould have M(%f)< -L(%f) < 0\nN_iter=%d\n", 
         FuncName, mu, lambda, mu, -lambda, N_iter);
   }
   
   fin_next = fin_orig;
   switch (d_order) {
      case SUMA_COLUMN_MAJOR:
         for (niter=0; niter < N_iter; ++niter) {
            if ( niter % 2 ) { /* odd */
               fin = fin_next; /* input from previous output buffer */
               fout = fout_final; /* results go into final vector */
               fin_next = fout_final; /* in the next iteration, the input is from fout_final */
            } else { /* even */
               /* input data is in fin_new */
               fin = fin_next;
               fout = fbuf; /* results go into buffer */
               fin_next = fbuf; /* in the next iteration, the input is from the buffer */
            }
            for (k=0; k < vpn; ++k) {
               n_offset = k * SO->N_Node;  /* offset of kth node value in fin */
               for (n=0; n < SO->N_Node; ++n) {
                  vnk = n+n_offset; 
                  fp = fin[vnk]; /* kth value at node n */
                  dfp = 0.0;
                  for (j=0; j < SO->FN->N_Neighb[n]; ++j) { /* calculating the laplacian */
                     fpj = fin[SO->FN->FirstNeighb[n][j]+n_offset]; /* value at jth neighbor of n */
                     if (wgt) dfp += wgt[n][j] * (fpj - fp); 
                     else dfp += (fpj - fp); /* will apply equal weight later */
                  }/* for j*/
                  if (niter%2) { /* odd */
                     if (wgt) fout[vnk] = fin[vnk] + mu * dfp;
                     else fout[vnk] = fin[vnk] + mu * dfp / (float)SO->FN->N_Neighb[n];   /* apply equal weight factor here */
                  }else{ /* even */
                    if (wgt) fout[vnk] = fin[vnk] + lambda * dfp;
                    else fout[vnk] = fin[vnk] + lambda * dfp / (float)SO->FN->N_Neighb[n];  /* apply equal weight factor here */
                  }      
               }/* for n */   
            }/* for k */
         }/* for niter */
         break;
      case SUMA_ROW_MAJOR:
         for (niter=0; niter < N_iter; ++niter) {
            if ( niter % 2 ) { /* odd */
               fin = fin_next; /* input from previous output buffer */
               fout = fout_final; /* results go into final vector */
               fin_next = fout_final; /* in the next iteration, the input is from fout_final */
            } else { /* even */
               /* input data is in fin_new */
               fin = fin_next;
               fout = fbuf; /* results go into buffer */
               fin_next = fbuf; /* in the next iteration, the input is from the buffer */
            }
            for (n=0; n < SO->N_Node; ++n) {
               vnk = n * vpn; /* index of 1st value at node n */
               for (k=0; k < vpn; ++k) {
                  fp = fin[vnk]; /* kth value at node n */
                  dfp = 0.0;
                  for (j=0; j < SO->FN->N_Neighb[n]; ++j) { /* calculating the laplacian */
                     fpj = fin[SO->FN->FirstNeighb[n][j]*vpn+k]; /* value at jth neighbor of n */
                     if (wgt) dfp += wgt[n][j] * (fpj - fp); 
                     else dfp += (fpj - fp); /* will apply equal weight later */
                  }/* for j*/
                  if (niter%2) { /* odd */
                     if (wgt) fout[vnk] = fin[vnk] + mu * dfp;
                     else fout[vnk] = fin[vnk] + mu * dfp / (float)SO->FN->N_Neighb[n];   /* apply equal weight factor here */
                  }else{ /* even */
                    if (wgt) fout[vnk] = fin[vnk] + lambda * dfp;
                    else fout[vnk] = fin[vnk] + lambda * dfp / (float)SO->FN->N_Neighb[n];  /* apply equal weight factor here */
                  }
                  ++vnk; /* index of next value at node n */
               } /* for k */
            }/* for n */
         }/* for niter */
         break;
      default:
         SUMA_SL_Err("Bad Major, very bad.\n");
         SUMA_RETURN(NULL);
         break;
   }
   
   if (fbuf) SUMA_free(fbuf); fbuf = NULL;
      
   SUMA_RETURN(fout);
}

/*!
   \brief Filter data defined on the surface using M.K. Chung et al.'s method (Neuroimage 03)
   dm_smooth = SUMA_Chung_Smooth (SO, wgt, N_iter, FWHM, fin, vpn, d_order, fout_user);
   
   \param SO (SUMA_SurfaceObject *) Surface object with valid 
                                    SO->NodeList, SO->FaceSetList and SO->FN 
   \param wgt (float **) interpolation weights for each node. 
                           These weights are obtained from SUMA_Chung_Smooth_Weights 
   \param N_iter (int) number of smoothing iterations (must be even, > 1) 
   \param FWHM (float) Full Width at Half Maximum of equivalent Gaussian filter
   \param fin (float *) vector containing node data. The length of this vector
                        is vpn x SO->N_Node , where vpn is the number of values
                        per node. 
   \param vpn (int) the numberof values per node in fin 
   \param d_order (SUMA_INDEXING_ORDER) Indicates how multiple values per node are stored in fin
                        SUMA_ROW_MAJOR: The data in fin is stored in *** Row Major *** order.
                        The ith value (start at 0) for node n is at index fin[vpn*n+i]
                        SUMA_COLUMN_MAJOR: The data in fin is stored in *** Column Major *** order.
                        The ith (start at 0) value for node n is at index fin[n+SO->N_Node*i]; 
                        etc...
   \param fout_user (float *) a pointer to the vector where the smoothed version of fin will reside
                        You can pass NULL and the function will do the allocation for you and return 
                        the pointer to the smoothed data in dm_smooth.
                        If you already have space allocated for the result, then pass the pointer
                        in fout_user and save on allocation time and space. In that case, dm_smooth
                        is equal to fout_user.
                        Either way, you are responsible for freeing memory pointed to by dm_smooth
                        DO NOT PASS fout_user = fin 
   \return dm_smooth (float *) A pointer to the smoothed data (vpn * SO->N_Node values). 
                        You will have to free the memory allocated for dm_smooth yourself.
                        
   \sa SUMA_Chung_Smooth_Weights
                        
*/

float * SUMA_Chung_Smooth (SUMA_SurfaceObject *SO, float **wgt, 
                           int N_iter, float FWHM, float *fin_orig, 
                           int vpn, SUMA_INDEXING_ORDER d_order, float *fout_final_user)
{
   static char FuncName[]={"SUMA_Chung_Smooth"};
   float *fout_final = NULL, *fbuf=NULL, *fin=NULL, *fout=NULL, *fin_next = NULL;
   float delta_time, fp, dfp, fpj, minfn=0.0, maxfn=0.0;
   int n , k, j, niter, vnk, os; 
   SUMA_Boolean LocalHead = YUP;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   if (N_iter % 2) {
      SUMA_SL_Err("N_iter must be an even number\n");
      SUMA_RETURN(NULL);
   }
   
   if (!SO || !wgt || !fin_orig) {
      SUMA_SL_Err("NULL SO or wgt or fin_orig\n");
      SUMA_RETURN(NULL);
   }
   if (!SO->FN) {
      SUMA_SL_Err("NULL SO->FN\n");
      SUMA_RETURN(NULL);
   }
   
   if (vpn < 1) {
      SUMA_SL_Err("vpn < 1\n");
      SUMA_RETURN(NULL);
   }  
   
   if (fout_final_user == fin_orig) {
      SUMA_SL_Err("fout_final_user == fin_orig");
      SUMA_RETURN(NULL);
   }
   
   if (!fout_final_user) { /* allocate for output */
      fout_final = (float *)SUMA_calloc(SO->N_Node * vpn, sizeof(float));
      if (!fout_final) {
         SUMA_SL_Crit("Failed to allocate for fout_final\n");
         SUMA_RETURN(NULL);
      }
   }else {
      fout_final = fout_final_user; /* pre-allocated */
   }
   
   /* allocate for buffer */
   fbuf = (float *)SUMA_calloc(SO->N_Node * vpn, sizeof(float));
   if (!fbuf) {
      SUMA_SL_Crit("Failed to allocate for fbuf\n");
      SUMA_RETURN(NULL);
   }
   
   
   fin_next = fin_orig;
   delta_time= (FWHM * FWHM)/(16*N_iter*log(2));
   switch (d_order) {
      case SUMA_COLUMN_MAJOR:
         for (niter=0; niter < N_iter; ++niter) {
            if ( niter % 2 ) { /* odd */
               fin = fin_next; /* input from previous output buffer */
               fout = fout_final; /* results go into final vector */
               fin_next = fout_final; /* in the next iteration, the input is from fout_final */
            } else { /* even */
               /* input data is in fin_new */
               fin = fin_next;
               fout = fbuf; /* results go into buffer */
               fin_next = fbuf; /* in the next iteration, the input is from the buffer */
            }
            for (k=0; k < vpn; ++k) {
               os = SO->N_Node*k;   /* node value indexing offset */
               for (n=0; n < SO->N_Node; ++n) {
                  vnk = n+os; /* index of kth value at node n */
                  fp = fin[vnk]; /* kth value at node n */
                  dfp = 0.0;
                  if (SO->FN->N_Neighb[n]) minfn = maxfn = fin[SO->FN->FirstNeighb[n][0]+os];
                  for (j=0; j < SO->FN->N_Neighb[n]; ++j) {
                     fpj = fin[SO->FN->FirstNeighb[n][j]+os]; /* value at jth neighbor of n */
                     if (fpj < minfn) minfn = fpj;
                     if (fpj > maxfn) maxfn = fpj;
                     dfp += wgt[n][j] * (fpj - fp); 
                  }/* for j*/
                  fout[vnk] = fin[vnk] + delta_time * dfp;
                  if (fout[vnk] < minfn) fout[vnk] = minfn;
                  if (fout[vnk] > maxfn) fout[vnk] = maxfn;
               }/* for n */   
            } /* for k */
         }/* for niter */
         break;
      case SUMA_ROW_MAJOR:
         SUMA_SL_Err("Row Major not implemented");
         SUMA_RETURN(NULL);
         break;
      default:
         SUMA_SL_Err("Bad Major, very bad.\n");
         SUMA_RETURN(NULL);
         break;
   }
   
   if (fbuf) SUMA_free(fbuf); fbuf = NULL;
               
   SUMA_RETURN(fout);
}
#ifdef SUMA_SurfSmooth_STAND_ALONE
void usage_SUMA_SurfSmooth ()
   {
      printf ("\n\33[1mUsage: \33[0m SurfSmooth <-i_TYPE inSurf> <-met method> <-output out.1D>\n"
              "\t Method specific options:\n"
              "\t LB_FEM:\n"
              "\t    <-input inData.1D> <-fwhm f>\n"
              "\t LM:\n"
              /*"\t                   [<-lim dmax>]\n"*/
              "\t    [<-kpb k>]\n"
              "\t Common optional options:\n"
              "\t -output out.1D: Name of output file. \n"
              "\t                 The default is inData_sm.1D with -LB_FEM method\n"
              "\t                 and NodeList_sm.1D with -lM method.\n" 
              "\t -Niter N: number of smoothing iterations.\n"
              "\t [-dbg_n ni dbg_prfx] [-h/-help]\n\n"
              "\t -i_TYPE inSurf specifies the input surface, TYPE is one of the following:\n"
              "\t    fs: FreeSurfer surface. \n"
              "\t        Only .asc surfaces are read.\n"
              "\t    sf: SureFit surface. \n"
              "\t        You must specify the .coord followed by the .topo file.\n"
              "\t    vec: Simple ascii matrix format. \n"
              "\t         You must specify the NodeList file followed by the FaceSetList file.\n"
              "\t         NodeList contains 3 floats per line, representing X Y Z vertex coordinates.\n"
              "\t         FaceSetList contains 3 ints per line, representing v1 v2 v3 triangle vertices.\n"
              "\t    ply: PLY format, ascii or binary.\n"
              "\t -input inData.1D: file containing data (in 1D format)\n"
              "\t                Each column in inData.1D is processed separately.\n"
              "\t                The number of rows must equal the number of\n"
              "\t                nodes in the surface. You can select certain\n"
              "\t                columns using the [] notation adopted by AFNI's\n"
              "\t                programs.\n"
              "\t -met method: name of smoothing method to use. Choose from:\n"
              "\t              LB_FEM: The method by Chung et al. 03.\n"
              "\t                      This method is used for filtering \n"
              "\t                      data on the surface not for smoothing the\n"
              "\t                      surface's geometry per se.\n"
              "\t              LM: The smoothing method proposed by G. Taubin 2000\n"
              "\t                  This method can be used for smoothing\n"
              "\t                  a surface's geometry.\n" 
              "\t -Niter N; Number of iterations (default is 100)\n"
              "\t Options for LB_FEM:\n"
              "\t -fwhm f: Full width at half maximum of equivalent Gaussian filter\n"
              /*"\t -lim dmax: maximum distance to search from a certain node \n"*/
              "\t Options for LM:\n"
              "\t -kpb k: Band pass frequency (default is 0.1).\n"
              "\t         values should be in the range 0 < k < 10\n"
              "\t         -lm and -kpb options are mutually exclusive.\n"
              "\t -lm l m: Lambda and Mu parameters. Sample values are:\n"
              "\t          0.6307 and -.6732\n"
              "\t          -lm and -kpb options are mutually exclusive.\n"
              "\t -dbg_n ni dbg_prfx: Output debugging results for node ni\n"
              "\t                     into files with prefix dbg_prfx.\n"
              "\t -h or -help: this help message.\n"
              "\t References: \n"
              "\t (1) M.K. Chung et al. Deformation-based surface morphometry\n"
              "\t applied to gray matter deformation. Neuroimage 18 (2003) 198-213\n"
              "\t (2) G. Taubin. Mesh Signal Processing. Eurographics 2000.\n\n"
              "\t See Also:   \n"
              "\t    ScaleToMap  to colorize the output and then load into SUMA\n"
              "\t Try:\n"
              "\t SurfSmooth -i_fs ../SurfData/SUMA/lh.smoothwm.asc -input in.1D -lim 15 -dbg_n 5 junk\n"
              "\t ScaleToMap -input junkoffset.1D 0 2 -cmap BGYR19 -nomsk_col > junkoffset.1D.col\n"
              "\n\t\t Ziad S. Saad SSCC/NIMH/NIH ziad@nih.gov \t Fri Nov 14 \n");
       exit (0);
   }

typedef enum { SUMA_NO_METH, SUMA_LB_FEM, SUMA_LM, SUMA_BRUTE_FORCE} SUMA_SMOOTHING_METHODS;

typedef struct {
   float lim;
   float fwhm;
   float kpb;
   float l;
   float m;
   int ShowNode;
   int Method;
   int dbg;
   int N_iter;
   SUMA_SO_File_Type iType;
   char *vp_name;
   char *sv_name;
   char *if_name;
   char *if_name2;
   char *in_name;
   char *out_name;   /* this one's dynamically allocated so you'll have to free it yourself */
   char *ShowOffset_DBG;
} SUMA_SURFSMOOTH_OPTIONS;

/*!
   \brief parse the arguments for SurfSmooth program
   
   \param argv (char *)
   \param argc (int)
   \return Opt (SUMA_SURFSMOOTH_OPTIONS *) options structure.
               To free it, use 
               SUMA_free(Opt->out_name); 
               SUMA_free(Opt);
*/
SUMA_SURFSMOOTH_OPTIONS *SUMA_SurfSmooth_ParseInput (char *argv[], int argc)
{
   static char FuncName[]={"SUMA_SurfSmooth_ParseInput"}; 
   SUMA_SURFSMOOTH_OPTIONS *Opt=NULL;
   int kar;
   char *outname;
   SUMA_Boolean brk = NOPE;
   SUMA_Boolean LocalHead = NOPE;

   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   Opt = (SUMA_SURFSMOOTH_OPTIONS *)SUMA_malloc(sizeof(SUMA_SURFSMOOTH_OPTIONS));
   
   kar = 1;
   Opt->lim = 1000000.0;
   Opt->fwhm = -1;
   Opt->ShowNode = -1;
   Opt->Method = SUMA_NO_METH;
   Opt->dbg = 0;
   Opt->if_name = NULL;
   Opt->if_name2 = NULL;
   Opt->in_name = NULL;
   Opt->out_name = NULL;
   Opt->vp_name = NULL; 
   Opt->sv_name = NULL;
   Opt->ShowOffset_DBG = NULL;
   Opt->iType = SUMA_FT_NOT_SPECIFIED;
   Opt->N_iter = 100;
   Opt->kpb = -1.0;
   Opt->l = -1.0;
   Opt->m = -1.0;
   outname = NULL;
	brk = NOPE;
	while (kar < argc) { /* loop accross command ine options */
		/*fprintf(stdout, "%s verbose: Parsing command line...\n", FuncName);*/
		if (strcmp(argv[kar], "-h") == 0 || strcmp(argv[kar], "-help") == 0) {
			 usage_SUMA_SurfSmooth();
          exit (0);
		}
		
      if (!brk && (strcmp(argv[kar], "-dbg_n") == 0)) {
         kar ++;
			if (kar+1 >= argc)  {
		  		fprintf (SUMA_STDERR, "need 2 arguments after -dbg_n ");
				exit (1);
			}
			Opt->ShowNode = atoi(argv[kar]); kar ++;
         Opt->ShowOffset_DBG = argv[kar];
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-Niter") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need 1 arguments after -Niter ");
				exit (1);
			}
			Opt->N_iter = atoi(argv[kar]); 
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-kpb") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need 1 arguments after -kpb ");
				exit (1);
			}
         if (Opt->l != -1.0  || Opt->m != -1.0) {
            fprintf (SUMA_STDERR, "options -lm and -kpb are mutually exclusive\n");
				exit (1);
         }
			Opt->kpb = atof(argv[kar]); 
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-lm") == 0)) {
         kar ++;
			if (kar+1 >= argc)  {
		  		fprintf (SUMA_STDERR, "need 2 arguments after -lm ");
				exit (1);
			}
         if (Opt->kpb != -1.0) {
            fprintf (SUMA_STDERR, "options -lm and -kpb are mutually exclusive\n");
				exit (1);
         }
			Opt->l = atof(argv[kar]); kar ++;
         Opt->m = atof(argv[kar]);  
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-input") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -input ");
				exit (1);
			}
			Opt->in_name = argv[kar];
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-output") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -output ");
				exit (1);
			}
			outname = argv[kar];
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-i_fs") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -i_fs ");
				exit (1);
			}
			Opt->if_name = argv[kar];
         Opt->iType = SUMA_FREE_SURFER;
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-i_sf") == 0)) {
         kar ++;
			if (kar+1 >= argc)  {
		  		fprintf (SUMA_STDERR, "need 2 argument after -i_sf");
				exit (1);
			}
			Opt->if_name = argv[kar]; kar ++;
         Opt->if_name2 = argv[kar];
         Opt->iType = SUMA_SUREFIT;
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-i_vec") == 0)) {
         kar ++;
			if (kar+1 >= argc)  {
		  		fprintf (SUMA_STDERR, "need 2 argument after -i_vec");
				exit (1);
			}
			Opt->if_name = argv[kar]; kar ++;
         Opt->if_name2 = argv[kar];
         Opt->iType = SUMA_VEC;
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-i_ply") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -i_ply ");
				exit (1);
			}
			Opt->if_name = argv[kar];
         Opt->iType = SUMA_PLY;
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-lim") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -lim ");
				exit (1);
			}
			Opt->lim = atof(argv[kar]);
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-fwhm") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -fwhm ");
				exit (1);
			}
			Opt->fwhm = atof(argv[kar]);
			brk = YUP;
		}
      
      if (!brk && (strcmp(argv[kar], "-met") == 0)) {
         kar ++;
			if (kar >= argc)  {
		  		fprintf (SUMA_STDERR, "need argument after -met ");
				exit (1);
			}
			if (strcmp(argv[kar], "LB_FEM") == 0)  Opt->Method = SUMA_LB_FEM;
         else if (strcmp(argv[kar], "LM") == 0)  Opt->Method = SUMA_LM;
         else {
            fprintf (SUMA_STDERR, "Method %s not supported.\n", argv[kar]);
				exit (1);
         }
			brk = YUP;
		}
      
      if (!brk) {
			fprintf (SUMA_STDERR,"Error %s:\nOption %s not understood. Try -help for usage\n", FuncName, argv[kar]);
			exit (1);
		} else {	
			brk = NOPE;
			kar ++;
		}
   }

   if (Opt->N_iter < 1) {
      fprintf (SUMA_STDERR,"Error %s:\nWith -Niter N option, N must be > 1\n", FuncName);
      exit (1);
   }
   
   if (Opt->ShowNode < 0 && Opt->ShowOffset_DBG) {
      fprintf (SUMA_STDERR,"Error %s:\nBad debug node index (%d) in option -dbg_n\n", FuncName, Opt->ShowNode);
      exit (1);
   }
   
   if (!Opt->if_name) {
      fprintf (SUMA_STDERR,"Error %s:\ninput surface not specified.\n", FuncName);
      exit(1);
   }

   if (Opt->iType == SUMA_FT_NOT_SPECIFIED) {
      fprintf (SUMA_STDERR,"Error %s:\ninput type not recognized.\n", FuncName);
      exit(1);
   }
   
   if (Opt->iType == SUMA_SUREFIT) {
      if (!Opt->if_name2) {
         fprintf (SUMA_STDERR,"Error %s:\ninput SureFit surface incorrectly specified.\n", FuncName);
         exit(1);
      }
   }
   
   if (Opt->iType == SUMA_VEC) {
      if (!Opt->if_name2) {
         fprintf (SUMA_STDERR,"Error %s:\ninput vec surface incorrectly specified.\n", FuncName);
         exit(1);
      }
   }
   
   /* test for existence of input files */
   if (!SUMA_filexists(Opt->if_name)) {
      fprintf (SUMA_STDERR,"Error %s:\n%s not found.\n", FuncName, Opt->if_name);
      exit(1);
   }
   
   if (Opt->in_name && !SUMA_filexists(Opt->in_name)) {
      fprintf (SUMA_STDERR,"Error %s:\n%s not found.\n", FuncName, Opt->if_name);
      exit(1);
   }
   
   if (Opt->if_name2) {
      if (!SUMA_filexists(Opt->if_name2)) {
         fprintf (SUMA_STDERR,"Error %s:\n%s not found.\n", FuncName, Opt->if_name2);
         exit(1);
      }
   }

   if (Opt->Method == SUMA_NO_METH) {
      fprintf (SUMA_STDERR,"Error %s:\nNo method was specified.\n", FuncName);
      exit(1);  
   }
   
   if (outname) {
      if (SUMA_filexists(outname)) {
         fprintf (SUMA_STDERR,"Error %s:\noutput file %s exists.\n", FuncName, outname);
         exit(1);
      }
      Opt->out_name = SUMA_copy_string(outname);
   } else {
      switch (Opt->Method) {
         case SUMA_LB_FEM:
            /* form autoname  */
            Opt->out_name = SUMA_Extension(Opt->in_name, ".1D", YUP); /*remove .1D */
            Opt->out_name = SUMA_append_replace_string(Opt->out_name,"_sm", "", 1); /* add _sm to prefix */
            Opt->out_name = SUMA_append_replace_string(Opt->out_name,".1D", "", 1); /* add .1D */
            break;
         case SUMA_LM:
            /* form autoname  */
            Opt->out_name = SUMA_copy_string("NodeList_sm.1D");
            break;
         default:
            fprintf (SUMA_STDERR,"Error %s:\nNot ready for this option here.\n", FuncName);
            exit(1);
            break;
      }
      if (SUMA_filexists(Opt->out_name)) {
         fprintf (SUMA_STDERR,"Error %s:\noutput file %s exists.\n", FuncName, Opt->out_name);
         exit(1);
      }
   }

   /* method specific checks */
   switch (Opt->Method) {
      case SUMA_LB_FEM:
         if (!Opt->in_name) {
            fprintf (SUMA_STDERR,"Error %s:\ninput data not specified.\n", FuncName);
            exit(1);
         }
         if (Opt->fwhm ==  -1.0) {
            fprintf (SUMA_STDERR,"Error %s:\n-fwhm option must be used with -met LB_FEM.\n", FuncName); 
            exit(1);
         }else if (Opt->fwhm <= 0.0) {
            fprintf (SUMA_STDERR,"Error %s:\nFWHM must be > 0\n", FuncName);
            exit(1);
         }
         if (Opt->kpb >= 0) {
            fprintf (SUMA_STDERR,"Error %s:\n-kpb option is not valid with -met LB_FEM.\n", FuncName); 
            exit(1);
         }         
         break;
      case SUMA_LM:
         
         if ( (Opt->l != -1.0 || Opt->m != -1.0) && Opt->kpb != -1.0) {
            fprintf (SUMA_STDERR,"Error %s:\nYou cannot mix options -kpb and -lm \n", FuncName);
            exit(1);
         }
         if (Opt->kpb != -1.0 && (Opt->kpb < 0.000001 || Opt->kpb > 10)) {
            fprintf (SUMA_STDERR,"Error %s:\nWith -kpb k option, you should satisfy 0 < k < 10\n", FuncName);
            exit(1);
         }
         if (Opt->l == -1.0 && Opt->m == -1.0 && Opt->kpb == -1.0) {
            Opt->kpb = 0.1;
         }

         if (Opt->l == -1.0 || Opt->m == -1.0) { /* convert kpb into l and m */
            if (!SUMA_Taubin_Smooth_Coef (Opt->kpb, &(Opt->l), &(Opt->m))) {
               SUMA_SL_Err("Failed to find smoothing coefficients");
               exit(1);            
            }
         } 

         if (Opt->in_name) {
            fprintf (SUMA_STDERR,"Error %s:\nOption -input not valid with -met LM.\n", FuncName);
            exit(1);
         }
         if (Opt->fwhm !=  -1.0) {
            fprintf (SUMA_STDERR,"Error %s:\nOption -fwhm not valid with -met LM.\n", FuncName);
            exit(1);
         }
         break;
      default:
         fprintf (SUMA_STDERR,"Error %s:\nNot ready for this option here.\n", FuncName);
         exit(1);
         break;
   }
   
   SUMA_RETURN (Opt);
}

int main (int argc,char *argv[])
{/* Main */    
   static char FuncName[]={"SurfSmooth"}; 
	int kar, icol, nvec, ncol=0, i, ii;
   float *data_old = NULL, *far = NULL;
   float **DistFirstNeighb;
   void *SO_name = NULL;
   SUMA_SurfaceObject *SO = NULL;
   MRI_IMAGE *im = NULL;
   SUMA_SFname *SF_name = NULL;
   struct  timeval start_time, start_time_all;
   float etime_GetOffset, etime_GetOffset_all;   
   SUMA_GET_OFFSET_STRUCT *OffS = NULL;
   SUMA_SURFSMOOTH_OPTIONS *Opt;  
   FILE *fileout=NULL; 
   float **wgt=NULL, *dsmooth=NULL;
   SUMA_INDEXING_ORDER d_order;
   SUMA_Boolean LocalHead = YUP;
   
	/* allocate space for CommonFields structure */
	SUMAg_CF = SUMA_Create_CommonFields ();
	if (SUMAg_CF == NULL) {
		fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_Create_CommonFields\n", FuncName);
		exit(1);
	}
   
   if (argc < 6)
       {
          usage_SUMA_SurfSmooth();
          exit (1);
       }
   
   Opt = SUMA_SurfSmooth_ParseInput (argv, argc);
   
   
   /* now for the real work */
   /* prepare the name of the surface object to read*/
   switch (Opt->iType) {
      case SUMA_SUREFIT:
         SF_name = (SUMA_SFname *) SUMA_malloc(sizeof(SUMA_SFname));
         sprintf(SF_name->name_coord,"%s", Opt->if_name);
         sprintf(SF_name->name_topo,"%s", Opt->if_name2); 
         if (!Opt->vp_name) { /* initialize to empty string */
            SF_name->name_param[0] = '\0'; 
         }
         else {
            sprintf(SF_name->name_param,"%s", Opt->vp_name);
         }
         SO_name = (void *)SF_name;
         fprintf (SUMA_STDOUT,"Reading %s and %s...\n", SF_name->name_coord, SF_name->name_topo);
         SO = SUMA_Load_Surface_Object (SO_name, SUMA_SUREFIT, SUMA_ASCII, Opt->sv_name);
         break;
      case SUMA_VEC:
         SF_name = (SUMA_SFname *) SUMA_malloc(sizeof(SUMA_SFname));
         sprintf(SF_name->name_coord,"%s", Opt->if_name);
         sprintf(SF_name->name_topo,"%s", Opt->if_name2); 
         SO_name = (void *)SF_name;
         fprintf (SUMA_STDOUT,"Reading %s and %s...\n", SF_name->name_coord, SF_name->name_topo);
         SO = SUMA_Load_Surface_Object (SO_name, SUMA_VEC, SUMA_ASCII, Opt->sv_name);
         break;
      case SUMA_FREE_SURFER:
         SO_name = (void *)Opt->if_name; 
         fprintf (SUMA_STDOUT,"Reading %s ...\n",Opt->if_name);
         SO = SUMA_Load_Surface_Object (SO_name, SUMA_FREE_SURFER, SUMA_ASCII, Opt->sv_name);
         break;  
      case SUMA_PLY:
         SO_name = (void *)Opt->if_name; 
         fprintf (SUMA_STDOUT,"Reading %s ...\n",Opt->if_name);
         SO = SUMA_Load_Surface_Object (SO_name, SUMA_PLY, SUMA_FF_NOT_SPECIFIED, Opt->sv_name);
         break;  
      default:
         fprintf (SUMA_STDERR,"Error %s: Bad format.\n", FuncName);
         exit(1);
   }
   
   if (!SO) {
      fprintf (SUMA_STDERR,"Error %s: Failed to read input surface.\n", FuncName);
      exit (1);
   }

   if (Opt->ShowNode >= 0 && Opt->ShowNode >= SO->N_Node) {
      fprintf (SUMA_STDERR,"Error %s: Requesting debugging info for a node index (%d) \n"
                           "that does not exist in a surface of %d nodes.\nRemember, indexing starts at 0.\n", 
                           FuncName, Opt->ShowNode, SO->N_Node);
      exit (1);
   }
   
   /* form the output filename, checking is done in parsing function */
      
   /* form EL and FN */
   if (!SUMA_SurfaceMetrics_eng (SO, "EdgeList", NULL, 0)) {
      SUMA_SLP_Err("Failed to calculate surface metrics");
      exit(1); 
   }

   
  
   switch (Opt->Method) {
      case SUMA_LB_FEM: 
         /* Moo Chung's method for interpolation weights */
         {
            /* now load the input data */
            im = mri_read_1D (Opt->in_name);

            if (!im) {
               SUMA_SL_Err("Failed to read 1D file");
               exit(1);
            }

            far = MRI_FLOAT_PTR(im);
            nvec = im->nx;
            ncol = im->ny;
            d_order = SUMA_COLUMN_MAJOR;

            if (!nvec) {
               SUMA_SL_Err("Empty file");
               exit(1);
            }
            
            SUMA_etime(&start_time,0);
            wgt = SUMA_Chung_Smooth_Weights(SO);
            if (!wgt) {
               SUMA_SL_Err("Failed to compute weights.\n");
               exit(1);
            }
            
            etime_GetOffset = SUMA_etime(&start_time,1);
            fprintf(SUMA_STDERR, "%s: weight computation took %f seconds for %d nodes.\n"
                                 "Projected time per 100000 nodes is: %f minutes\n", 
                                       FuncName, etime_GetOffset, SO->N_Node, 
                                       etime_GetOffset * 100000 / 60.0 / (SO->N_Node));
            
            dsmooth = SUMA_Chung_Smooth (SO, wgt, Opt->N_iter, Opt->fwhm, far, ncol, SUMA_COLUMN_MAJOR, NULL);
            
            etime_GetOffset = SUMA_etime(&start_time,1);
            fprintf(SUMA_STDERR, "%s: Total processing took %f seconds for %d nodes.\n"
                                 "Projected time per 100000 nodes is: %f minutes\n", 
                                       FuncName, etime_GetOffset, SO->N_Node, 
                                       etime_GetOffset * 100000 / 60.0 / (SO->N_Node));
            /* write out the results */
            fileout = fopen(Opt->out_name, "w");
            SUMA_disp_vecmat (dsmooth, SO->N_Node, ncol, 1, d_order, fileout, YUP);
            fclose(fileout); fileout = NULL;

            if (wgt) SUMA_free2D ((char **)wgt, SO->N_Node); wgt = NULL;
            if (dsmooth) SUMA_free(dsmooth); dsmooth = NULL;
         }
         break;
         
      case SUMA_LM:
         /* Taubin's */
         {
            
            d_order =  SUMA_ROW_MAJOR; 
            dsmooth = SUMA_Taubin_Smooth (SO, NULL, 
                            Opt->l, Opt->m, SO->NodeList, 
                            Opt->N_iter, 3, d_order,
                            NULL); 


            /* write out the results */
            fileout = fopen(Opt->out_name, "w");
            SUMA_disp_vecmat (dsmooth, SO->N_Node, 3, 1, d_order, fileout, 0);
            fclose(fileout); fileout = NULL;

            if (dsmooth) SUMA_free(dsmooth); dsmooth = NULL;

         }
         break;
         
      case SUMA_BRUTE_FORCE:
         /* a method that will likely be dropped NOT FINISHED*/
            {
            /* initialize OffS */
            OffS = SUMA_Initialize_getoffsets (SO->N_Node);

            SUMA_etime(&start_time_all,0);
            for (i=0; i < SO->N_Node; ++i) {
               /* show me the offset from node 0 */
               if (LocalHead) fprintf(SUMA_STDERR,"%s: Calculating offsets from node %d\n",FuncName, i);
               if (i == 0) {
                  SUMA_etime(&start_time,0);
               }
               SUMA_getoffsets2 (i, SO, Opt->lim, OffS);
               if (i == 99) {
                  etime_GetOffset = SUMA_etime(&start_time,1);
                  fprintf(SUMA_STDERR, "%s: Search to %f mm took %f seconds for %d nodes.\n"
                                       "Projected completion time: %f minutes\n", 
                                       FuncName, Opt->lim, etime_GetOffset, i+1, 
                                       etime_GetOffset * SO->N_Node / 60.0 / (i+1));
               }

               /* Show me the offsets for one node*/
               if (Opt->ShowOffset_DBG) {
                  if (i == Opt->ShowNode) {
                     FILE *fid=NULL;
                     char *outname=NULL;
                     outname = SUMA_Extension(Opt->ShowOffset_DBG, ".1D", YUP);
                     outname = SUMA_append_replace_string(outname, "offset.1D", "", 1);
                     fid = fopen(outname, "w"); free(outname); outname = NULL;
                     if (!fid) {
                        SUMA_SL_Err("Could not open file for writing.\nCheck file permissions, disk space.\n");
                     } else {
                        fprintf (fid,"#Column 1 = Node index\n"
                                     "#column 2 = Neighborhood layer\n"
                                     "#Column 3 = Distance from node %d\n", Opt->ShowNode);
                        for (ii=0; ii<SO->N_Node; ++ii) {
                           if (OffS->LayerVect[ii] >= 0) {
                              fprintf(fid,"%d\t%d\t%f\n", ii, OffS->LayerVect[ii], OffS->OffVect[ii]);
                           }
                        }
                        fclose(fid);
                     }
                  }
               }

               if (LocalHead) fprintf(SUMA_STDERR,"%s: Recycling OffS\n", FuncName);
               SUMA_Recycle_getoffsets (OffS);
               if (LocalHead) fprintf(SUMA_STDERR,"%s: Done.\n", FuncName); 

            }

            etime_GetOffset_all = SUMA_etime(&start_time_all,1);
            fprintf(SUMA_STDERR, "%s: Done.\nSearch to %f mm took %f minutes for %d nodes.\n" , 
                                 FuncName, Opt->lim, etime_GetOffset_all / 60.0 , SO->N_Node);

         }
         break;
      default:
         SUMA_SL_Err("Bad method, should not be here.");
         exit(1);
         break;
   }
   
   /* smooth each column on its own ...*/  
   /* the way you read and pass the data to the smoothing function should be 
   designed, this is at a debugging level ... */
   
   mri_free(im); im = NULL;   /* done with that baby */
   if (SO) SUMA_Free_Surface_Object(SO);
   if (data_old) SUMA_free(data_old);  
   if (Opt->out_name) free(Opt->out_name); Opt->out_name = NULL;
   if (Opt) SUMA_free(Opt);
   exit(0);
}
#endif

/*!
   Given a set of node indices, return a patch of the original surface that contains them
   
   Patch = SUMA_getPatch (NodesSelected, N_Nodes, Full_FaceSetList, N_Full_FaceSetList, Memb, MinHits)

   \param NodesSelected (int *) N_Nodes x 1 Vector containing indices of selected nodes. 
            These are indices into NodeList making up the surface formed by Full_FaceSetList.
   \param N_Nodes (int) number of elements in NodesSelected
   \param Full_FaceSetList (int *) N_Full_FaceSetList  x 3 vector containing the triangles forming the surface 
   \param N_Full_FaceSetList (int) number of triangular facesets forming the surface
   \param Memb (SUMA_MEMBER_FACE_SETS *) structure containing the node membership information (result of SUMA_MemberFaceSets function)
   \param MinHits (int) minimum number of nodes to be in a patch before the patch is selected. 
         Minimum is 1, Maximum logical is 3, assuming you do not have repeated node indices
         in NodesSelected.
   \ret Patch (SUMA_PATCH *) Structure containing the patch's FaceSetList, FaceSetIndex (into original surface) and number of elements.
         returns NULL in case of trouble.   Free Patch with SUMA_freePatch(Patch);

   \sa SUMA_MemberFaceSets, SUMA_isinbox, SUMA_PATCH
*/

SUMA_PATCH * SUMA_getPatch (  int *NodesSelected, int N_Nodes, 
                              int *Full_FaceSetList, int N_Full_FaceSetList, 
                              SUMA_MEMBER_FACE_SETS *Memb, int MinHits)
{
   int * BeenSelected;
   int i, j, node, ip, ip2, NP;
   SUMA_PATCH *Patch;
   static char FuncName[]={"SUMA_getPatch"};
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   NP = 3;
   BeenSelected = (int *)SUMA_calloc (N_Full_FaceSetList, sizeof(int));
   Patch = (SUMA_PATCH *)SUMA_malloc(sizeof(SUMA_PATCH));
   
   if (!BeenSelected || !Patch) {
      fprintf (SUMA_STDERR,"Error %s: Could not allocate for BeenSelected or patch.\n", FuncName);
      SUMA_RETURN(NULL);
   }
   /* find out the total number of facesets these nodes are members of */
   Patch->N_FaceSet = 0; /* total number of facesets containing these nodes */
   for (i=0; i < N_Nodes; ++i) {
      node = NodesSelected[i];
      for (j=0; j < Memb->N_Memb[node]; ++j) {
         if (!BeenSelected[Memb->NodeMemberOfFaceSet[node][j]]) {
            /* this faceset has not been selected, select it */
            ++ Patch->N_FaceSet;
         }
         ++ BeenSelected[Memb->NodeMemberOfFaceSet[node][j]];
      }   
   }
   
   /* now load these facesets into a new matrix */
   
   Patch->FaceSetList = (int *) SUMA_calloc (Patch->N_FaceSet * 3, sizeof(int));
   Patch->FaceSetIndex = (int *) SUMA_calloc (Patch->N_FaceSet, sizeof(int));
   Patch->nHits = (int *) SUMA_calloc (Patch->N_FaceSet, sizeof(int));
   
   if (!Patch->FaceSetList || !Patch->FaceSetIndex || !Patch->nHits) {
      fprintf (SUMA_STDERR,"Error %s: Could not allocate for Patch->FaceSetList || Patch_FaceSetIndex.\n", FuncName);
      SUMA_RETURN(NULL);
   }
   
   j=0;
   for (i=0; i < N_Full_FaceSetList; ++i) {
      if (BeenSelected[i] >= MinHits) {
         Patch->nHits[j] = BeenSelected[i];
         Patch->FaceSetIndex[j] = i;
         ip = NP * j;
         ip2 = NP * i;
         Patch->FaceSetList[ip] = Full_FaceSetList[ip2];
         Patch->FaceSetList[ip+1] = Full_FaceSetList[ip2+1];
         Patch->FaceSetList[ip+2] = Full_FaceSetList[ip2+2];
         ++j;
      }
   }
   
   /* reset the numer of facesets because it might have changed given the MinHits condition,
   It won't change if MinHits = 1.
   It's OK not to change the allocated space as long as you are using 1D arrays*/
   Patch->N_FaceSet = j;
   
   if (BeenSelected) SUMA_free(BeenSelected);
   
   SUMA_RETURN(Patch);   
}

/*!
   ans = SUMA_freePatch (SUMA_PATCH *Patch) ;
   frees Patch pointer 
   \param Patch (SUMA_PATCH *) Surface patch pointer
   \ret ans (SUMA_Boolean)
   \sa SUMA_getPatch
*/

SUMA_Boolean SUMA_freePatch (SUMA_PATCH *Patch) 
{
   static char FuncName[]={"SUMA_freePatch"};
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   
   if (Patch->FaceSetIndex) SUMA_free(Patch->FaceSetIndex);
   if (Patch->FaceSetList) SUMA_free(Patch->FaceSetList);
   if (Patch->nHits) SUMA_free(Patch->nHits);
   if (Patch) SUMA_free(Patch);
   SUMA_RETURN(YUP);
   
}

SUMA_Boolean SUMA_ShowPatch (SUMA_PATCH *Patch, FILE *Out) 
{
   static char FuncName[]={"SUMA_freePatch"};
   int ip, i;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   if (!Out) Out = stderr;
   
   fprintf (Out, "Patch Contains %d triangles:\n", Patch->N_FaceSet);
   fprintf (Out, "FaceIndex (nHits): FaceSetList[0..2]\n");
   for (i=0; i < Patch->N_FaceSet; ++i) {
      ip = 3 * i;   
      fprintf (Out, "%d(%d):   %d %d %d\n",
            Patch->FaceSetIndex[i], Patch->nHits[i], Patch->FaceSetList[ip],
            Patch->FaceSetList[ip+1], Patch->FaceSetList[ip+2]);
   }
   
   SUMA_RETURN(YUP);
}

/*!
   \brief Returns the contour of a patch 
*/
SUMA_CONTOUR_EDGES * SUMA_GetContour (SUMA_SurfaceObject *SO, int *Nodes, int N_Node, int *N_ContEdges)
{
   static char FuncName[]={"SUMA_GetContour"};
   SUMA_EDGE_LIST * SEL=NULL;
   SUMA_PATCH *Patch = NULL;
   int i, Tri, Tri1, Tri2, sHits;
   SUMA_CONTOUR_EDGES *CE = NULL;
   SUMA_Boolean *isNode=NULL, LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   *N_ContEdges = -1;
   
   /* get the Node member structure if needed*/
   if (!SO->MF) {
      SUMA_SLP_Err("Member FaceSet not created.\n");
      SUMA_RETURN(CE);
      #if 0
         fprintf(SUMA_STDOUT, "%s: Computing MemberFaceSets... \n", FuncName);
         SO->MF = SUMA_MemberFaceSets (SO->N_Node, SO->FaceSetList, SO->N_FaceSet, 3);
         if (SO->MF == NULL) {
            fprintf(SUMA_STDERR, "Error %s: Failed in SUMA_MemberFaceSets. \n", FuncName);
            SUMA_RETURN(CE);
         }
         /* YOU SHOULD CREATE INODES FOR THIS BABY and link related surfaces,
         actually, you might want to do this on the parent surface and then link
         Inodes to other related surfaces. Perhaps add it at startup */
      #endif
   }  

   /* create a flag vector of which node are in Nodes */
   isNode = (SUMA_Boolean *) SUMA_calloc(SO->N_Node, sizeof(SUMA_Boolean));
   if (!isNode) {
      SUMA_SLP_Crit("Failed to allocate for isNode");
      SUMA_RETURN(CE);
   }
   
   for (i=0; i < N_Node; ++i) isNode[Nodes[i]] = YUP;
   
   Patch = SUMA_getPatch (Nodes, N_Node, SO->FaceSetList, SO->N_FaceSet, SO->MF, 2);
   if (LocalHead) SUMA_ShowPatch (Patch,NULL);
   
   if (Patch->N_FaceSet) {
      SEL = SUMA_Make_Edge_List (Patch->FaceSetList, Patch->N_FaceSet, SO->N_Node, SO->NodeList);
   
      /* SUMA_Show_Edge_List (SEL, NULL); */
      /* allocate for maximum */
      CE = (SUMA_CONTOUR_EDGES *) SUMA_malloc(SEL->N_EL * sizeof(SUMA_CONTOUR_EDGES));
      if (!CE) {
         SUMA_SLP_Crit("Failed to allocate for CE");
         SUMA_RETURN(CE);
      }
   
      /* edges that are part of unfilled triangles are good */
      i = 0;
      *N_ContEdges = 0;
      while (i < SEL->N_EL) {
         if (SEL->ELps[i][2] == 2) {
            Tri1 = SEL->ELps[i][1];
            Tri2 = SEL->ELps[i+1][1];
            sHits = Patch->nHits[Tri1] + Patch->nHits[Tri2];
            if (sHits == 5 || sHits == 4) { /* one tri with 3 hits and one with 2 hits or 2 Tris with 2 hits each */
               /* Pick edges that are part of only one triangle with three hits */
                                           /* or two triangles with two hits */
               /* There's one more condition, both nodes have to be a part of the original list */
               if (isNode[SEL->EL[i][0]] && isNode[SEL->EL[i][1]]) {
                  CE[*N_ContEdges].n1 = SEL->EL[i][0];
                  CE[*N_ContEdges].n2 = SEL->EL[i][1];
                  ++ *N_ContEdges;
                  
                  if (LocalHead) {
                     fprintf (SUMA_STDERR,"%s: Found edge made up of nodes [%d %d]\n",
                        FuncName, SEL->EL[i][0], SEL->EL[i][1]);
                  }
               }
            }
         }
         
         if (SEL->ELps[i][2] > 0) {
            i += SEL->ELps[i][2];
         } else {
            i ++;
         }
      }
      
      /* Now reallocate */
      if (! *N_ContEdges) {
         SUMA_free(CE); CE = NULL;
         SUMA_RETURN(CE);
      }else {
         CE = (SUMA_CONTOUR_EDGES *) SUMA_realloc (CE, *N_ContEdges * sizeof(SUMA_CONTOUR_EDGES));
         if (!CE) {
            SUMA_SLP_Crit("Failed to reallocate for CE");
            SUMA_RETURN(CE);
         }
      }
         
      SUMA_free_Edge_List (SEL); 
   }
   
   SUMA_freePatch (Patch);
   SUMA_free(isNode);
   
   SUMA_RETURN(CE);
}

/*!
 
From File : Plane_Equation.c
Author : Ziad Saad
Date : Thu Nov 19 14:55:54 CST 1998
 
\brief   finds the plane passing through 3 points
 
 
Usage : 
      Eq = SUMA_Plane_Equation (P1, P2, P3)
 
 
   \param P1 (float *) 1x3 vector Coordinates of Point1
   \param P2 (float *) 1x3 vector Coordinates of Point2
   \param P3 (float *) 1x3 vector Coordinates of Point3
   \return Eq (float *) 1x4 vector containing the equation of the plane containing the three points.
         The equation of the plane is : 
         Eq[0] X + Eq[1] Y + Eq[2] Z + Eq[3] = 0
   
         If the three points are colinear, Eq = [0 0 0 0]
 
*/
float * SUMA_Plane_Equation (float * P1, float *P2, float *P3)
{/*SUMA_Plane_Equation*/
   float *Eq;
   static char FuncName[] = {"SUMA_Plane_Equation"}; 
    
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   Eq = (float *) SUMA_calloc(4,sizeof(float));
   if (!Eq)
      {
         fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
         SUMA_RETURN (NULL);
      }
   
   Eq[0] = P1[1] * (P2[2]-P3[2]) 
         + P2[1] * (P3[2]-P1[2]) 
         + P3[1] * (P1[2]-P2[2]);
         
   Eq[1] = P1[2] * (P2[0]-P3[0]) 
         + P2[2] * (P3[0]-P1[0]) 
         + P3[2] * (P1[0]-P2[0]);
         
   Eq[2] = P1[0] * (P2[1]-P3[1]) 
         + P2[0] * (P3[1]-P1[1]) 
         + P3[0] * (P1[1]-P2[1]);
         
   Eq[3] =  - P1[0] * (P2[1] * P3[2] - P3[1] * P2[2]) 
            - P2[0] * (P3[1] * P1[2] - P1[1] * P3[2]) 
            - P3[0] * (P1[1] * P2[2] - P2[1] * P1[2]);

   SUMA_RETURN (Eq);
}/*SUMA_Plane_Equation*/

/*! 
 
\brief Determines the intersection of a plane and a surface 

 
   
   SPI = SUMA_Surf_Plane_Intersect (SO, PlaneEq)

\param SO (SUMA_SurfaceObject *) Pointer to surface object structure.
\param  PlaneEq (float *) : 4x1 vector containing the 4 coefficients of the equation 
                     of the plane to intersect the surface
                     PlaneEq[0] X + PlaneEq[1] Y + PlaneEq[2] Z + PlaneEq[3] = 0
\return SPI (SUMA_SURF_PLANE_INTERSECT *) Pointer to intersection structure. See help on fields in SUMA_define.h
                                             NULL in case of error.
     
 

\sa an older matlab version in Surf_Plane_Intersect_2.m  
\sa SUMA_PlaneEq
\sa SUMA_Allocate_SPI
\sa SUMA_free_SPI
    
   
*/
 
SUMA_SURF_PLANE_INTERSECT *SUMA_Surf_Plane_Intersect (SUMA_SurfaceObject *SO, float *PlaneEq)
{/*SUMA_Surf_Plane_Intersect*/
   static char FuncName[]={"SUMA_Surf_Plane_Intersect"};
   int  i, k , k3, i3, n1, n2;
   float DT_ABVBEL, DT_POSNEG, u;
   float *NodePos;
   SUMA_SURF_PLANE_INTERSECT *SPI;
   struct  timeval start_time, start_time2;
   SUMA_Boolean LocalHead = NOPE;
   SUMA_Boolean  Hit;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   /* Start timer for next function */
   SUMA_etime(&start_time2,0);
      
   if (LocalHead)   fprintf(SUMA_STDERR, "%s : Determining intersecting segments ...\n", FuncName);
   
   /* allocate for the return structure.
   NOTE: If (in a different form of this function) you do not allocate for SPI each time you call the function, make sure you reset all 
   elements of the following vector fields: 
   IsTriHit[] = NOPE;
   TriBranch[] = 0
   */
   SPI = SUMA_Allocate_SPI (SO);
   if (!SPI) {
      fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_Allocate_SPI\n", FuncName);
      SUMA_RETURN (SPI);
   }
   
   /* allocate for temporary stuff */
   NodePos = (float *) SUMA_calloc (SO->N_Node , sizeof(float));

   if (!NodePos )
      {
         fprintf (SUMA_STDERR, "Error %s: Could not allocate in SUMA_Surf_Plane_Intersect\n", FuncName);
         SUMA_free_SPI (SPI); SPI = NULL;
         SUMA_RETURN (SPI);
      }

      
   /* Start timer for next function */
   SUMA_etime(&start_time,0);
                           
   /* Find out which nodes are above and which are below the plane */
   for (k=0;k<SO->N_Node; ++k)
      {
         k3 = 3*k;
         NodePos[k] = PlaneEq[0] * SO->NodeList[k3] + PlaneEq[1] * SO->NodeList[k3+1] 
                     +PlaneEq[2] * SO->NodeList[k3+2] + PlaneEq[3] ;
      }
      
   /* stop timer */
   DT_ABVBEL = SUMA_etime(&start_time,1);
                              
   
   /*
      NodePos is < 0 for nodes below the plane and > 0 for points above the plane
      Go through each connection and determine if it intersects the plane
      If a segment intersects the surface, it means that the sign
      of p  would be <= 0 (each point is on a different side of the plane)
   */
   
   /* Start timer for next function */
   SUMA_etime(&start_time,0);
   
   /*
      Determine the segments intersecting the surface,
      The triangles that contain these segments.
      The nodes that form the intersected segments 
   */
   SPI->N_IntersEdges = 0;
   SPI->N_IntersTri = 0;
   SPI->N_NodesInMesh = 0;
   k=0; 
   Hit = NOPE;
   while (k < SO->EL->N_EL)
      {
         /* find out if segment intersects */
         if (SUMA_IS_NEG(NodePos[SO->EL->EL[k][0]] * NodePos[SO->EL->EL[k][1]])) {
            Hit = YUP;
            /* find the intersection point in that segment */
            u = -NodePos[SO->EL->EL[k][0]] / (NodePos[SO->EL->EL[k][1]] - NodePos[SO->EL->EL[k][0]]);
            i3 = 3 * k;
            n1 = 3 * SO->EL->EL[k][0];
            n2 = 3 * SO->EL->EL[k][1];
            
            SPI->IntersNodes[i3] = SO->NodeList[n1] + u * ( SO->NodeList[n2] - SO->NodeList[n1] ); ++i3; ++n2; ++n1;
            SPI->IntersNodes[i3] = SO->NodeList[n1] + u * ( SO->NodeList[n2] - SO->NodeList[n1] ); ++i3; ++n2; ++n1;
            SPI->IntersNodes[i3] = SO->NodeList[n1] + u * ( SO->NodeList[n2] - SO->NodeList[n1] ); ++i3; ++n2; ++n1;
            
            /* 
            fprintf (SUMA_STDERR,"%s: Edge %d, IntersNodes[%d]= [%f, %f, %f]\n", 
               FuncName, k, 3*k, SPI->IntersNodes[3*k], SPI->IntersNodes[3*k+1], SPI->IntersNodes[3*k+2]);
            */
               
            /* Store the intersected segment */
            SPI->IntersEdges[SPI->N_IntersEdges] = k;
            ++SPI->N_IntersEdges;
            
            /* mark this segment in the boolean vector to speed up some other functions */
            SPI->isEdgeInters[k] = YUP;
                  
            /* Store the index of the triangle hosting this edge*/
            if (!SPI->isTriHit[SO->EL->ELps[k][1]]) {
               SPI->IntersTri[SPI->N_IntersTri] = SO->EL->ELps[k][1];
               ++(SPI->N_IntersTri);
               SPI->isTriHit[SO->EL->ELps[k][1]] = YUP;
            }
            
            /* mark the nodes forming the intersection edges */
            if (!SPI->isNodeInMesh[SO->EL->EL[k][0]]) {
               SPI->isNodeInMesh[SO->EL->EL[k][0]] = YUP;
               ++(SPI->N_NodesInMesh);
            }
            if (!SPI->isNodeInMesh[SO->EL->EL[k][1]]) {
               SPI->isNodeInMesh[SO->EL->EL[k][1]] = YUP;
               ++(SPI->N_NodesInMesh);
            } 
         } else {
            Hit = NOPE;
         }
            
            /*  skip ahead of duplicate edge listings */
            if (SO->EL->ELps[k][2] > 0) {
               if (Hit) { /* you must mark these triangles */
                  i3 = 3 * k;
                  for (i=1; i < SO->EL->ELps[k][2]; ++i) {
                     SPI->isEdgeInters[k+i] = YUP;
                     n1 = 3 * (k+i);
                     SPI->IntersNodes[n1] = SPI->IntersNodes[i3]; ++i3; ++n1;
                     SPI->IntersNodes[n1] = SPI->IntersNodes[i3]; ++i3; ++n1;
                     SPI->IntersNodes[n1] = SPI->IntersNodes[i3]; ++i3; ++n1;
                     /*
                     fprintf (SUMA_STDERR,"%s: Edge %d, IntersNodes[%d]= [%f, %f, %f]\n", 
                        FuncName, k+i, n1, SPI->IntersNodes[3*(k+i)], SPI->IntersNodes[3*(k+i)+1], SPI->IntersNodes[3*(k+i)+2]);
                     */
                     if (!SPI->isTriHit[SO->EL->ELps[k+i][1]]) {
                        SPI->IntersTri[SPI->N_IntersTri] = SO->EL->ELps[k+i][1];
                        ++(SPI->N_IntersTri);
                        SPI->isTriHit[SO->EL->ELps[k+i][1]] = YUP;   
                     }
                  }
               }
               k += SO->EL->ELps[k][2];
            } else ++k;
      }
            
      
   /* stop timer */
   DT_POSNEG = SUMA_etime(&start_time,1);
   
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Found %d intersect segments, %d intersected triangles, %d nodes in mesh (exec time %f + %f = %f secs).\n", 
      FuncName, SPI->N_IntersEdges, SPI->N_IntersTri, SPI->N_NodesInMesh, DT_ABVBEL, DT_POSNEG, DT_ABVBEL + DT_POSNEG);
   
/* free locally allocated memory */
if (NodePos) SUMA_free (NodePos);
   

SUMA_RETURN (SPI);
}/*SUMA_Surf_Plane_Intersect*/

/*!
   \brief a wrapper function for SUMA_Surf_Plane_Intersect that returns the intersection 
   in the form of an ROI datum
   
   \param SO (SUMA_SurfaceObject *)
   \param Nfrom (int) index of node index on SO from which the path should begin
   \param Nto (int) index of node index on SO where the path will end
   \param P (float *) XYZ of third point that will form the cutting plane with Nfrom and Nto's coordinates
            This point is usually, the Near Plane clipping point of Nto's picking line.
   \return ROId (SUMA_ROI_DATUM *) pointer to ROI datum structure which contains the NodePath from Nfrom to Nto
   along with other goodies.      
*/
SUMA_ROI_DATUM *SUMA_Surf_Plane_Intersect_ROI (SUMA_SurfaceObject *SO, int Nfrom, int Nto, float *P)
{
   static char FuncName[]={"SUMA_Surf_Plane_Intersect_ROI"};
   SUMA_ROI_DATUM *ROId=NULL;
   SUMA_Boolean LocalHead = NOPE;
   int N_left;
   SUMA_SURF_PLANE_INTERSECT *SPI = NULL;
   SUMA_ROI *ROIe = NULL, *ROIt = NULL, *ROIn = NULL, *ROIts = NULL;
   float *Eq = NULL;
   /* The 3 flags below are for debugging. */
   SUMA_Boolean DrawIntersEdges=NOPE; /* Draw edges intersected by plane   */
   SUMA_Boolean DrawIntersTri = NOPE; /* Draw triangles intersected by plane */
   SUMA_Boolean DrawIntersNodeStrip = NOPE; /* Draw intersection node strip which is the shortest path between beginning and ending nodes */ 
   SUMA_Boolean DrawIntersTriStrip=NOPE; /* Draw intersection triangle strip which is the shortest path between beginning and ending nodes */
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   /* computing plane's equation */
   Eq = SUMA_Plane_Equation ( &(SO->NodeList[3*Nfrom]), 
                              P,
                              &(SO->NodeList[3*Nto]) );
   
   if (!Eq) {
      fprintf(SUMA_STDOUT,"Error %s: Failed in SUMA_Plane_Equation.\n", FuncName);
      SUMA_RETURN(ROId);
   } 
   
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Computing Intersection with Surface.\n", FuncName);
   SPI = SUMA_Surf_Plane_Intersect (SO, Eq);
   if (!SPI) {
      fprintf(SUMA_STDOUT,"Error %s: Failed in SUMA_Surf_Plane_Intersect.\n", FuncName);
      SUMA_RETURN(ROId);
   }
   
   if (DrawIntersEdges) {
      /* Show all intersected edges */
      ROIe =  SUMA_AllocateROI (SO->idcode_str, SUMA_ROI_EdgeGroup, "SurfPlane Intersection - Edges", SPI->N_IntersEdges, SPI->IntersEdges);
      if (!ROIe) {
         fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_AllocateROI.\n", FuncName);
      } else {
         if (!SUMA_AddDO (SUMAg_DOv, &SUMAg_N_DOv, (void *)ROIe, ROIO_type, SUMA_LOCAL)) {
            fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_AddDO.\n", FuncName);
         }
      }
   }
   
   if (DrawIntersTri) {
      /* Show all intersected triangles */
      ROIt =  SUMA_AllocateROI (SO->idcode_str, SUMA_ROI_FaceGroup, "SurfPlane Intersection - Triangles", SPI->N_IntersTri, SPI->IntersTri);
      if (!ROIt) {
         fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_AllocateROI.\n", FuncName);
      } else {
         if (!SUMA_AddDO (SUMAg_DOv, &SUMAg_N_DOv, (void *)ROIt, ROIO_type, SUMA_LOCAL)) {
            fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_AddDO.\n", FuncName);
         }
      }
   }

   /* create ROId */
   ROId = SUMA_AllocROIDatum ();
   ROId->Type = SUMA_ROI_NodeSegment;

   /* calculate shortest path */
   N_left = SPI->N_NodesInMesh;
   ROId->nPath = SUMA_Dijkstra (SO, Nfrom, Nto, SPI->isNodeInMesh, &N_left, 1, &(ROId->nDistance), &(ROId->N_n));
   if (ROId->nDistance < 0 || !ROId->nPath) {
      fprintf(SUMA_STDERR,"\aError %s: Failed in fast SUMA_Dijkstra.\n*** Two points are not connected by intersection. Repeat last selection.\n", FuncName);

      /* clean up */
      if (SPI) SUMA_free_SPI (SPI); 
      SPI = NULL;
      if (ROId) SUMA_FreeROIDatum (ROId);
      SUMA_RETURN(NULL);   
   }
   
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Shortest inter nodal distance along edges between nodes %d <--> %d (%d nodes) is %f.\n", 
      FuncName, Nfrom, Nto, ROId->N_n, ROId->nDistance);
   

   #if 0
   
      /* FOR FUTURE USAGE:
         When drawing on the surface, it is possible to end up with node paths for which the 
         triangle strip tracing routines fail. That's paretly because it is possible for these
         node paths to visit one node more than once, eg: ... 34 23 34 .... 
         That is OK for drawing purposes but not for say, making measurements on the surface.
      */
      
      /* calculate shortest path along the intersection of the plane with the surface */
      /* get the triangle path corresponding to shortest distance between Nx and Ny */

      /* Old Method: Does not result is a strip of triangle that is continuous or connected
      by an intersected edge. Function is left here for historical reasons. 
         tPath = SUMA_NodePath_to_TriPath_Inters (SO, SPI, Path, N_Path, &N_Tri); */

      /* you should not need to go much larger than NodeDist except when you are going for 
      1 or 2 triangles away where discrete jumps in d might exceed the limit. 
      Ideally, you want this measure to be 1.5 NodeDist or say, 15 mm, whichever is less.... */

      /* THIS SHOULD BE OPTIONAL */
      ROId->tPath = SUMA_IntersectionStrip (SO, SPI, ROId->nPath, ROId->N_n, &(ROId->tDistance), 2.5 *ROId->nDistance, &(ROId->N_t));                      
      if (!ROId->tPath) {
         fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_IntersectionStrip. Proceeding\n", FuncName);
         /* do not stop here, it is OK if you can't find the triangle strip */
         /* if (ROId) SUMA_FreeROIDatum (ROId);
         SUMA_RETURN(NULL);   */
      } else {
         /* ROId->tPath has a potentially enourmous chunk of memory allocated for it. Trim the fat. */
         {
            int *tPath_tmp=NULL, i_tmp=0;
            tPath_tmp = (int *)SUMA_calloc (ROId->N_t, sizeof(int));
            if (!tPath_tmp) {
               SUMA_RegisterMessage (SUMAg_CF->MessageList, "Failed to allocate for tpath_tmp", FuncName, SMT_Critical, SMA_LogAndPopup);
               SUMA_RETURN(NULL);
            }
            for (i_tmp=0; i_tmp<ROId->N_t; ++i_tmp) tPath_tmp[i_tmp] = ROId->tPath[i_tmp];
            SUMA_free(ROId->tPath);
            ROId->tPath = tPath_tmp;
         } 

         fprintf (SUMA_STDERR, "%s: Shortest inter nodal distance along surface between nodes %d <--> %d is %f.\nTiangle 1 is %d\n", 
            FuncName, Nfrom, Nto, ROId->tDistance, ROId->tPath[0]);

         if (DrawIntersTriStrip) {
            /* Show intersected triangles, along shortest path */
            ROIts =  SUMA_AllocateROI (SO->idcode_str, SUMA_ROI_FaceGroup, "SurfPlane Intersection - Triangles- Shortest", ROId->N_t, ROId->tPath);
            if (!ROIts) {
               fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_AllocateROI.\n", FuncName);
               if (ROIn) SUMA_freeROI(ROIn);
               if (ROIts) SUMA_freeROI(ROIts);
               if (ROId) SUMA_FreeROIDatum (ROId);
               SUMA_RETURN(NULL);   
            }
            if (!SUMA_AddDO (SUMAg_DOv, &SUMAg_N_DOv, (void *)ROIts, ROIO_type, SUMA_LOCAL)) {
               fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_AddDO.\n", FuncName);
               if (ROIn) SUMA_freeROI(ROIn);
               if (ROIts) SUMA_freeROI(ROIts);
               if (ROId) SUMA_FreeROIDatum (ROId);
               SUMA_RETURN(NULL);
            }
         }

         if (ROId->nPath && DrawIntersNodeStrip) {
            #if 0
               /* Show me the Path */
               for (ii=0; ii < ROId->N_n; ++ii) fprintf(SUMA_STDERR," %d\t", ROId->nPath[ii]);
            #endif

            /* Show Path */
            ROIn =  SUMA_AllocateROI (SO->idcode_str, SUMA_ROI_NodeGroup, "SurfPlane Intersection - Nodes", ROId->N_n, ROId->nPath);
            if (!ROIn) {
               fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_AllocateROI.\n", FuncName);
               if (ROIn) SUMA_freeROI(ROIn);
               if (ROIts) SUMA_freeROI(ROIts);
               if (ROId) SUMA_FreeROIDatum (ROId);
               SUMA_RETURN(NULL);
            }
            if (!SUMA_AddDO (SUMAg_DOv, &SUMAg_N_DOv, (void *)ROIn, ROIO_type, SUMA_LOCAL)) {
               fprintf(SUMA_STDERR,"Error %s: Failed in SUMA_AddDO.\n", FuncName);
               if (ROIn) SUMA_freeROI(ROIn);
               if (ROIts) SUMA_freeROI(ROIts);
               if (ROId) SUMA_FreeROIDatum (ROId);
               SUMA_RETURN(NULL);
            }

         }
      }                        
   #endif
   
   if (LocalHead) fprintf(SUMA_STDERR,"%s: Freeing Eq...\n", FuncName);
   if (Eq) SUMA_free(Eq);

   if (LocalHead) fprintf(SUMA_STDERR,"%s: Freeing SPI...\n", FuncName);
   if (SPI) SUMA_free_SPI (SPI);
   
   if (LocalHead) fprintf(SUMA_STDERR,"%s:Done Freeing...\n", FuncName);      
   
   SUMA_RETURN(ROId);
}

/*!

\brief 
SBv =  SUMA_AssignTriBranch (SO, SPI, Nx, BranchCount, DoCopy)
\param SO (SUMA_SurfaceObject *) Pointer to Surface Object structure 
\param SPI (SUMA_SURF_PLANE_INTERSECT *) Pointer to Surface Plane Intersection structure 
\param Nx (int) Node index to start the first branch at. This parameter is optional. pass -1 if you do not want it set.
\param BranchCount (int *) Pointer to the total number of branches found.
\param DoCopy (SUMA_Boolean) flag indicating whether to preserve (YUP) the values in SPI->IntersEdges and SPI->N_IntersEdges or not (NOPE).
                             If you choose YUP, a copy of SPI->IntersEdges is made and manipulated. 
\return Bv (SUMA_TRI_BRANCH*) Pointer to a vector of *BranchCount branches formed by the intersections. 
      A branch is formed by a series of connected triangles. A Branch can be a loop but that is not determined in this function.
      NULL if trouble is encountered. 
      On some surfaces with cuts you may have a branch split in two which requires welding. 
      No such thing is done here but a warning is printed out.
                  
NOTE: The vector SPI->IntersEdges is modified by this function. 
\sa SUMA_free_STB for freeing Bv
*/

#define SUMA_MAX_BRANCHES 300

/* assign a branch to each triangle intersected */
SUMA_TRI_BRANCH* SUMA_AssignTriBranch (SUMA_SurfaceObject *SO, SUMA_SURF_PLANE_INTERSECT *SPI, 
                                       int Nx, int *BranchCount, SUMA_Boolean DoCopy)
{
   static char FuncName[]={"SUMA_AssignTriBranch"};
   int *IntersEdgesCopy = NULL, N_IntersEdgesCopy, i_Branch, E1, kedge, i, 
         N_iBranch[SUMA_MAX_BRANCHES], NBlist[SUMA_MAX_BRANCHES], iBranch = 0, 
         N_Branch, Bcnt, ilist, j, ivisit, *VisitationOrder, TriCheck;
   SUMA_Boolean Local_IntersEdgesCopy = NOPE;
   int *TriBranch = NULL; /*!< Vector of SO->EL->N_EL / 3 elements but with only N_IntersTri meaningful values. If TriBranch[j] = b
                     then triangle j in SO->FaceSet is a member of Branch b. Branch numbering starts at 1 and may not be consecutive. 
                     A branch is a collection of connected triangles and may form a closed loop */
   SUMA_TRI_BRANCH *Bv = NULL;
   SUMA_Boolean LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);


   i_Branch = 0;
   
   /* make a copy of SPI->N_IntersEdges since this vector will be modified and might be needed later, 
      might make that optional later and just copy pointers if IntersEdgesCopy is not needed elsewhere.
      IntersEdgesCopy flag is used to decide on freeing IntersEdgesCopy or not.*/

   VisitationOrder = (int *)SUMA_calloc (SO->N_FaceSet, sizeof (int)); /* keeps track of the order in which triangles are visited. This is used for creating branches with the proper sequence */
   TriBranch = (int *)SUMA_calloc (SO->EL->N_EL / 3,  sizeof(int));
   
   if (!VisitationOrder || !TriBranch) {
      fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
      if (TriBranch) SUMA_free(TriBranch);
      if (VisitationOrder) SUMA_free(VisitationOrder);
      SUMA_RETURN (NULL);   
   }
   
   N_IntersEdgesCopy = SPI->N_IntersEdges;
   if (DoCopy) {
      IntersEdgesCopy = (int *) SUMA_calloc (N_IntersEdgesCopy, sizeof (int));
      Local_IntersEdgesCopy = YUP;
      for (i=0; i < N_IntersEdgesCopy; ++i) {
         IntersEdgesCopy[i] = SPI->IntersEdges[i];
      }
   }else {
      Local_IntersEdgesCopy = NOPE;
      IntersEdgesCopy = SPI->IntersEdges;
   }

   if (!IntersEdgesCopy) {
     fprintf (SUMA_STDERR, "Error %s: Failed to allocate for or receive IntersEdgesCopy.\n", FuncName);
     if (TriBranch) SUMA_free(TriBranch);
     if (VisitationOrder) SUMA_free(VisitationOrder);
     SUMA_RETURN (NULL);
   }

   ivisit = 0;
   while (N_IntersEdgesCopy) {
   
      if (!i_Branch && Nx >= 0) {
         /* start from edge containing Nx, this is only done at the starting point (i_Branch = 0) */
         E1 = -1;
         i=0;
         while (i < N_IntersEdgesCopy && E1 < 0) {
            if ( (SO->EL->EL[IntersEdgesCopy[i]][0] == Nx) || (SO->EL->EL[IntersEdgesCopy[i]][1] == Nx) ) {
               E1 = IntersEdgesCopy[i];
               kedge = i;
            }  
            ++i;
         }
      }else {
         /* no starting orders, start from any decent edge */
         /* find an edge with one hosting triangle */
         E1 = SUMA_Find_Edge_Nhost (SO->EL, IntersEdgesCopy, N_IntersEdgesCopy, &kedge, 1);
      }      

      if (E1 < 0) { /* no such edge found, take first edge in InInter */
            kedge = 0;
            E1 = IntersEdgesCopy[kedge];
            if (LocalHead) fprintf (SUMA_STDERR, "%s: No 1 host edge edge found.\n", FuncName);
      }else {
            if (LocalHead) fprintf (SUMA_STDERR, "%s: Found edge.\n", FuncName);
      }
      
      /* remove this edge from the list */
      --(N_IntersEdgesCopy);
      if (LocalHead) fprintf (SUMA_STDERR, "%s: kedge = %d, N_IntersEdgesCopy = %d.\n", FuncName, kedge, N_IntersEdgesCopy);
      IntersEdgesCopy[kedge] = IntersEdgesCopy[N_IntersEdgesCopy];

      /* start a new i_Branch - All i_Branch indices must be > 0*/
      ++i_Branch;   
      if (i_Branch > SUMA_MAX_BRANCHES-1) {
         fprintf (SUMA_STDERR, "Error %s: No more than %d branches allowed.\n", FuncName, SUMA_MAX_BRANCHES);
         SUMA_RETURN (NULL); 
      } 
      
      /* mark the triangle containing E1 */
      if (LocalHead) fprintf (SUMA_STDERR, "%s: Marking triangle %d with branch %d.\n", FuncName, SO->EL->ELps[E1][1], i_Branch);
      TriBranch[SO->EL->ELps[E1][1]] = i_Branch;
      VisitationOrder[ivisit] = SO->EL->ELps[E1][1]; ++ivisit;
      
      if (LocalHead) fprintf (SUMA_STDERR, "%s: Called recursive SUMA_Mark_Tri.\n", FuncName);
      if (!SUMA_Mark_Tri (SO->EL, E1, i_Branch, TriBranch, IntersEdgesCopy, &(N_IntersEdgesCopy), VisitationOrder, &ivisit)) {
         fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_Mark_Tri.\n", FuncName);
      }
      if (LocalHead) fprintf (SUMA_STDERR, "%s: Returned from recursive SUMA_Mark_Tri.\n", FuncName);

      /* repeat till all edges are used up */
   }

   if (Local_IntersEdgesCopy) {
      if (LocalHead) fprintf (SUMA_STDERR, "%s: freeing IntersEdgesCopy.\n", FuncName);
      SUMA_free(IntersEdgesCopy); 
   }else {
      /* also change N_IntersEdges */
      SPI->N_IntersEdges = N_IntersEdgesCopy;
   }

   /* SUMA_disp_dvect (TriBranch, SO->N_FaceSet);  */

   N_Branch = i_Branch;

   /* determine the number of branch elements to allocate for - IDIOT PROOF, doing it in the recursive function SUMA_Mark_Tri was annoying*/
   for (i=0; i <= N_Branch; ++i) N_iBranch[i] = 0; /* remember, Branch numbering starts at 1 */
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Searching all %d intersected triangles.\n", FuncName, SPI->N_IntersTri);
   Bcnt = 0;
   for (i=0; i < SO->N_FaceSet; ++i) {
      if (TriBranch[i]) {
         /* fprintf (SUMA_STDERR, "%d:%d\t", TriBranch[i], N_iBranch[TriBranch[i]]); */
         ++Bcnt;
         N_iBranch[TriBranch[i]] = N_iBranch[TriBranch[i]] + 1;
      }
   }
   
   #if 0
      fprintf (SUMA_STDERR, "Values in N_iBranch, idiot proof:\n");
      SUMA_disp_dvect (N_iBranch, N_Branch+1);
      fprintf (SUMA_STDERR, "\n");
   #endif
   
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Found %d triangles belonging to a branch out of %d intersected triangles.\n", FuncName, Bcnt, SPI->N_IntersTri);
     
   /* Now you want to create a vector of N_Branches to represent the intersection */
   Bv = (SUMA_TRI_BRANCH *) SUMA_malloc (sizeof(SUMA_TRI_BRANCH)*(N_Branch+1)); /* you should only need N_Branch, but that 1 won't hurt ...*/
   if (!Bv) {
      fprintf (SUMA_STDERR, "Error %s: Could not allocate for Bv.\n", FuncName);
      SUMA_RETURN (NULL);
   }
   
   /* initialize allocated Bv elements */
   for (i=0; i<= N_Branch; ++i) { /* You have allocated for N_Branch+1*/
      Bv[i].list = NULL;
      Bv[i].N_list = 0;
   } 
   
   Bcnt = 0;
   for (i=0; i<= N_Branch; ++i) { /* Branch numbering starts at 1 */
      if (N_iBranch[i]) {
         /* something in that branch, allocate and initialize*/
         if (LocalHead) fprintf (SUMA_STDERR, "%s: Allocating for %d elements, Old Branch %d, New Branch %d.\n", FuncName, N_iBranch[i], i, Bcnt); 
         Bv[Bcnt].list = (int *) SUMA_calloc (N_iBranch[i]+1, sizeof(int));
         Bv[Bcnt].N_list = N_iBranch[i];
         Bv[Bcnt].iBranch = Bcnt;
         NBlist[i] = Bcnt; /* store new indexing for Branches */
         ++Bcnt;
      }
      
   }
   
   /* store the total number of used branches */
   *BranchCount = Bcnt;
   
   /* now fill up the branches*/
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Filling up branches...\n", FuncName);
   for (i=0; i <= N_Branch; ++i) N_iBranch[i] = 0; /* now use this vector as a counter for the filling at each new branch index */
   for (i=0; i < SPI->N_IntersTri; ++i) { /* only go over visited triangles */
      TriCheck = TriBranch[VisitationOrder[i]]; /* returns the branch number of triangle VisitationOrder[i] */
      if (TriCheck) {
         Bcnt = NBlist[TriCheck]; /* get the new branch number from the original (old) branch number */
         #if 0
         fprintf (SUMA_STDERR,"%s: Tricheck = %d\n", FuncName, TriCheck); */
         if (Bcnt >= *BranchCount) {
            fprintf (SUMA_STDERR, "\aError %s: BranchCount = %d <= Bcnt = %d.\n", FuncName, *BranchCount, Bcnt);
         }
         if (N_iBranch[Bcnt] >= Bv[Bcnt].N_list) {
            fprintf (SUMA_STDERR, "\aError %s: Bcnt = %d. N_iBranch[Bcnt] = %d >= Bv[Bcnt].N_list = %d\n", FuncName, Bcnt, N_iBranch[Bcnt], Bv[Bcnt].N_list);
         }
         #endif
         Bv[Bcnt].list[N_iBranch[Bcnt]] = VisitationOrder[i]; /* store the index of the visited triangle in that branch */
         N_iBranch[Bcnt] += 1; /* store the number of elements in that branch */
      }
   }
   
   if (LocalHead) fprintf (SUMA_STDERR, "%s: freeing ...\n", FuncName);
   if (VisitationOrder) SUMA_free(VisitationOrder);
   if (TriBranch) SUMA_free(TriBranch);
   SUMA_RETURN (Bv);    
}

/*! 
   Function to show the contents of a SUMA_TRI_BRANCH structure
*/
SUMA_Boolean SUMA_show_STB (SUMA_TRI_BRANCH *B, FILE *Out)
{
   static char FuncName[]={"SUMA_show_STB"};
   int i;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
  
   if (!Out) Out = SUMA_STDERR;
   
   if (!B) {
      fprintf (Out, "%s: Empy structure.\n", FuncName);
   }
   
   fprintf (Out, "%s:\tBranch #%d. %d elements in list\nlist:\t", FuncName, B->iBranch, B->N_list);
   for (i=0; i < B->N_list; ++i) {
      fprintf (Out, "%d\t", B->list[i]);
   }
   fprintf (Out, "\n");
   
   SUMA_RETURN (YUP);
}

/*!
   Function to free a vector of SUMA_TRI_BRANCH structures.
*/

void SUMA_free_STB (SUMA_TRI_BRANCH *Bv, int N_Bv) 
{

   static char FuncName[]={"SUMA_free_STB"};
   int i;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   for (i=0; i < N_Bv; ++i) {
      if (Bv[i].list) SUMA_free(Bv[i].list);
   }
   if (Bv) SUMA_free(Bv);
   
   SUMA_RETURNe;
    
}

/*!
   \brief Allocates a structure for computing the intersection of a surface with a plane
   The allocation is done conservatively, expecting the worse case scenario. 
   
   \param SO (SUMA_SurfaceObject *) Surface Object structure used to get number of nodes, edges, etc ...
   \return SPI (SUMA_SURF_PLANE_INTERSECT *) Pointer to surface plane intersection structure.
         see structure definition for more info.

*/
SUMA_SURF_PLANE_INTERSECT * SUMA_Allocate_SPI (SUMA_SurfaceObject *SO) 
{
   static char FuncName[]={"SUMA_Allocate_SPI"};
   int i;
   SUMA_SURF_PLANE_INTERSECT *SPI = NULL;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   SPI = (SUMA_SURF_PLANE_INTERSECT *) SUMA_malloc(sizeof(SUMA_SURF_PLANE_INTERSECT));
   if (!SPI) {
      fprintf (SUMA_STDERR, "Error %s: Could not allocate for SPI\n", FuncName);
      SUMA_RETURN (SPI);
   }
   
   SPI->IntersEdges = (int *) SUMA_calloc (SO->EL->N_EL, sizeof(int)); /* allocate for the max imaginable*/
   SPI->IntersNodes = (float *) SUMA_calloc (3 * SO->EL->N_EL, sizeof(float));
   SPI->isEdgeInters = (SUMA_Boolean *) SUMA_calloc (SO->EL->N_EL, sizeof(SUMA_Boolean));
   SPI->IntersTri = (int *) SUMA_calloc (SO->N_FaceSet, sizeof(int));
   SPI->isNodeInMesh = (SUMA_Boolean *) SUMA_calloc (SO->N_Node, sizeof(SUMA_Boolean));
   SPI->isTriHit = (SUMA_Boolean *) SUMA_calloc (SO->N_FaceSet, sizeof(SUMA_Boolean));

   if (!SPI->IntersEdges || !SPI->IntersTri || !SPI->IntersNodes || !SPI->isTriHit || !SPI->isEdgeInters)
      {
         fprintf (SUMA_STDERR, "Error %s: Could not allocate \n", FuncName);
         SUMA_RETURN (SPI);
      }
   
   SPI->N_IntersEdges = 0;
   SPI->N_IntersTri = 0;
   SPI->N_NodesInMesh = 0;  
   SUMA_RETURN (SPI);
}

/*!
free the SPI structure    
*/
void SUMA_free_SPI (SUMA_SURF_PLANE_INTERSECT *SPI)
{
   static char FuncName[]={"SUMA_free_SPI"};
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   if (!SPI) SUMA_RETURNe;
   if (SPI->IntersTri) SUMA_free(SPI->IntersTri);
   if (SPI->IntersNodes) SUMA_free(SPI->IntersNodes);
   if (SPI->IntersEdges) SUMA_free(SPI->IntersEdges);
   if (SPI->isNodeInMesh) SUMA_free(SPI->isNodeInMesh); 
   if (SPI->isTriHit) SUMA_free (SPI->isTriHit);
   if (SPI->isEdgeInters) SUMA_free (SPI->isEdgeInters);
     
   if (SPI) SUMA_free(SPI);
   
   SUMA_RETURNe;
}

/*! 
Show the SPI structure 
*/
SUMA_Boolean SUMA_Show_SPI (SUMA_SURF_PLANE_INTERSECT *SPI, FILE * Out, SUMA_SurfaceObject *SO)
{
   static char FuncName[]={"SUMA_Show_SPI"};
   int i;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   if (!Out) Out = SUMA_STDERR;
   
   if (!SPI) {
      fprintf (Out,"Error %s: NULL POINTER.\n", FuncName);
   }
   
   fprintf (Out,"Intersection Edges: %d\n[", SPI->N_IntersEdges);
   for (i=0; i < SPI->N_IntersEdges; ++i) {
      fprintf (Out, "%d, %d\n", SO->EL->EL[SPI->IntersEdges[i]][0], SO->EL->EL[SPI->IntersEdges[i]][1]);
   }
   fprintf (Out," ]\n");
   
   fprintf (Out,"Intersection Nodes: %d\n[", SPI->N_IntersEdges);
   for (i=0; i < SO->EL->N_EL; ++i) {
      if (SPI->isEdgeInters[i]) fprintf (Out, "%f, %f, %f, ", SPI->IntersNodes[3*i], SPI->IntersNodes[3*i+1], SPI->IntersNodes[3*i+2]);
   }
   fprintf (Out," ]\n");
   
   fprintf (Out,"Intersected Triangles: %d\n[", SPI->N_IntersTri);
   for (i=0; i < SPI->N_IntersTri; ++i) {
      fprintf (Out, "t%d\t", SPI->IntersTri[i]);
   }
   fprintf (Out," ]\n");
   SUMA_RETURN(YUP);
}

#define NO_LOG
SUMA_Boolean SUMA_Mark_Tri (SUMA_EDGE_LIST  *EL, int E1, int iBranch, int *TriBranch, int *IsInter, int *N_IsInter, int *VisitationOrder, int *ivisit)
{
   static char FuncName[]={"SUMA_Mark_Tri"};
   int Tri = -1, Found, k, kedge = 0, E2, Ntri = 0;
   static int In = 0;
   SUMA_Boolean LocalHead = NOPE;
   
   /* this is a recursive function, you don't want to log every time it is called */
   /* if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);    */
   
   ++In;
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Entered #%d.\n", FuncName, In);
   
   /* find the next triangle hosting E1, if possible, otherwise it is the end of the branch. */
   if (EL->ELps[E1][2] != 2) { /* reached a dead end , end of branch */
      /* mark triangle, remove E1 from list and return */
      if (LocalHead) fprintf (SUMA_STDERR, "%s: reached end of branch.\n", FuncName);
      kedge = 0;
      Found = NOPE;
      while (!Found && kedge < *N_IsInter) {
         if (IsInter[kedge] == E1) {
            Found = YUP;
            *N_IsInter = *N_IsInter - 1;
            IsInter[kedge] = IsInter[*N_IsInter];
         } else ++kedge;
      }
      return (YUP);
   }else {
      Tri = EL->ELps[E1][1];
      if (TriBranch[Tri]) { /* try second triangle */
         Tri = EL->ELps[E1+1][1];
      }
      if (LocalHead) fprintf (SUMA_STDERR, "%s: moving on to triangle %d.\n", FuncName, Tri);
   }
   
   if (!TriBranch[Tri]) { 
      /* unvisited, mark with iBranch */
      TriBranch[Tri] = iBranch;
      VisitationOrder[*ivisit] = Tri;
      ++(*ivisit);
      /* find other edges in this triangle that have been intersected */
      Found = NOPE; 
      k = 0;
      while (!Found && k < 3) {
         E2 = EL->Tri_limb[Tri][k]; /* this may not be the first occurence of this edge since the list contains duplicates */
         if (LocalHead) {
            fprintf (SUMA_STDERR, "%s: Trying edge E2 %d (%d %d), tiangle %d, edge %d.\n", 
                     FuncName, E2, EL->EL[E2][0], EL->EL[E2][1], Tri, k);
         }
         while (EL->ELps[E2][2] < 0) { /* find the first occurence of this edge in the list */
            E2--;
         }
         if (LocalHead) fprintf (SUMA_STDERR, "%s: E2 changed to %d. E1 is %d\n", FuncName, E2, E1);
         if (E2 != E1) {
            /* was E2 intersected ? */
            kedge = 0;
            while (!Found && kedge < *N_IsInter) {
               if (IsInter[kedge] == E2) {
                  Found = YUP;
                  if (LocalHead) fprintf (SUMA_STDERR, "%s: E2 is intersected.\n", FuncName);
               }
               else ++kedge;
            }
         }
         ++k;
      }
      
      if (!Found) {
         fprintf (SUMA_STDERR, "Error %s: No second edge found.\n", FuncName);
         return (NOPE);
      } else {
         if (LocalHead) fprintf (SUMA_STDERR, "%s: Removing E2 from List and calling SUMA_Mark_Tri.\n", FuncName);
         /* remove this new edge from the list */
         *N_IsInter = *N_IsInter - 1;
         IsInter[kedge] = IsInter[*N_IsInter];
         
         /* continue visitation */
         if (!SUMA_Mark_Tri (EL, E2, iBranch, TriBranch, IsInter, N_IsInter, VisitationOrder, ivisit)) {
            fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_Mark_Tri.\n", FuncName);
            return (NOPE);
         } 
         return (YUP);
      }
   } else {
      if (TriBranch[Tri] != iBranch) {
         fprintf (SUMA_STDERR, "\a%s: Branches colliding, Must weld %d to %d.\n", FuncName, iBranch, TriBranch[Tri]);
         
         /* DO NOT MODIFY THE VALUE OF BRANCH or you will mistakingly link future branches*/ 
      }
      /* visited, end of branch return */
      if (LocalHead) fprintf (SUMA_STDERR, "%s: End of branch. Returning.\n", FuncName);
      return (YUP);
   }
   
   fprintf (SUMA_STDERR, "Error %s: Should not be here.\n", FuncName);
   return (NOPE);
}

/*!
   E = SUMA_Find_Edge_N_Host (EL, IsInter, N_IsInter, Nhost);
   \brief Finds an edge that has Nhost hosting triangles. Only the edges indexed in IsInter are examined 
   \param EL (SUMA_EDGE_LIST *) Complete Edge list structure for surface
   \param IsInter (int *) vector containing indices into EL->EL matrix which contains the EdgeList
   \param N_IsInter (int) number of elements in IsInter
   \param kedge (int *) pointer to index into IsInter where E was found
   \param Nhost number of hosting triangles (should be 2 for a closed surface, 1 for edge edges and more than 2 for errors in tessellation
   \return E (int) index into EL->EL of the first edge (of those listed in IsInter) encountered that has N hosting triangles.
      -1 is returned if no edges are found
   
   This function is meant to be used with SUMA_Surf_Plane_Intersect
*/
int SUMA_Find_Edge_Nhost (SUMA_EDGE_LIST  *EL, int *IsInter, int N_IsInter, int *i, int Nhost)
{
   static char FuncName[]={"SUMA_Find_Edge_Nhost"};
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   for (*i=0; *i < N_IsInter; ++(*i)) {
      if (EL->ELps[IsInter[*i]][2] == Nhost) SUMA_RETURN (IsInter[*i]);
   }
   
   SUMA_RETURN (-1);

}

/*! 
\brief Path = SUMA_Dijkstra (SO, Nx, Ny, isNodeInMesh, N_isNodeInMesh, Method_Number, Path_length, N_Path);
      Finds the shortest distance between nodes Nx and Ny on SO with a restriction on the number of nodes
      available for travel. In other terms, the search space is limited to a subset of the nodes forming SO. 
      The subset of nodes is stored in isNodeInMesh. This subset is typically specified in SUMA_Surf_Plane_Intersect.
      
      
 Path = SUMA_Dijkstra (SO, Nx,  Ny, isNodeInMesh, N_isNodeInMesh, Method_Number, Path_length, N_Path)
\param SO (SUMA_SurfaceObject *) The surface Object structure. NodeList, EL and FN are needed. 
\param Nx (int) The node index (referring to SO's nodes) where the search begins.
\param Ny (int) The node index (referring to SO's nodes) where the search ends.
\param isNodeInMesh (SUMA_Boolean *) Pointer to SO->N_Node long vector such that 
                                       if (isNodeInMesh[i]) then node i is part of the 
                                       mesh that is used in the search path. This mesh is a subset 
                                       of SO->FaceSetList and is typically obtained when one 
                                       runs SUMA_Surf_Plane_Intersect. Running SUMA_Dijkstra on 
                                       a complete surface is only for very patient people.
                                NOTE:  This vector is modified as a node is visited. Make sure you 
                                       do not use it after this function has been called.
\param N_isNodeInMesh (int *) Pointer to the total number of nodes that make up the mesh (subset of SO)
               This parameter is passed as a pointer because as nodes in the mesh are visited, that
               number is reduced and represents when the function returns, the number of nodes that were
               never visited in the search. 
\param Method_Number (int) selector for which algorithm to use. Choose from:
                     0 - Straight forward implementation, slow
                     1 - Variation to eliminate long searches for minimum of L, much much much faster than 0, 5 time more memory.
\param Path_length (float *) The distance between Nx and Ny. This value is negative if no path between Nx and Ny was found.
\param N_Path (int *) Number of nodes forming the Path vector

\return Path (float) A vector of N_Path node indices forming the shortest path, from Nx (Path[0]) to Ny (Path[*N_Path - 1]). 
                  NULL is returned in case of error.

\sa Graph Theory by Ronald Gould and labbook NIH-2 page 154 for path construction
*/
#define LARGE_NUM 9e300
/* #define LOCALDEBUG */ /* lots of debugging info. */
int * SUMA_Dijkstra (SUMA_SurfaceObject *SO, int Nx, int Ny, SUMA_Boolean *isNodeInMesh, int *N_isNodeInMesh, int Method_Number, float *Lfinal, int *N_Path)
{
   static char FuncName[] = {"SUMA_Dijkstra"};
   SUMA_Boolean LocalHead = NOPE;
   float *L = NULL, Lmin = -1.0, le = 0.0, DT_DIJKSTRA;
   int i, iw, iv, v, w, N_Neighb, *Path = NULL;
   struct  timeval  start_time;
   SUMA_DIJKSTRA_PATH_CHAIN *DC = NULL, *DCi, *DCp;
   SUMA_Boolean Found = NOPE;
   /* variables for method 2 */
   int N_Lmins, *vLmins, *vLocInLmins, iLmins, ReplacingNode, ReplacedNodeLocation;
   float *Lmins; 
   
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   *Lfinal = -1.0;
   *N_Path = 0;
   
   /* make sure Both Nx and Ny exist in isNodeInMesh */
   if (!isNodeInMesh[Nx]) {
      fprintf (SUMA_STDERR,"\aError %s: Node %d (Nx) is not in mesh.\n", FuncName, Nx);
      SUMA_RETURN (NULL);
   }  
   if (!isNodeInMesh[Ny]) {
      fprintf (SUMA_STDERR,"\aError %s: Node %d (Ny) is not in mesh.\n", FuncName, Ny);
      SUMA_RETURN (NULL);
   }

   if (!SO->FN) {
      fprintf (SUMA_STDERR, "Error %s: SO does not have FN structure.\n", FuncName);
      SUMA_RETURN (NULL);
   }

   if (LocalHead) {
      /* Start timer for next function */
      SUMA_etime(&start_time,0);      
   }
   
   /* allocate for chain */
   DC = (SUMA_DIJKSTRA_PATH_CHAIN *) SUMA_malloc (sizeof(SUMA_DIJKSTRA_PATH_CHAIN) * SO->N_Node);
   if (!DC) {
      fprintf (SUMA_STDERR, "Error %s: Could not allocate. \n", FuncName);
      SUMA_RETURN (NULL);
   }
   
   switch (Method_Number) {
   
      case 0:  /* Method 0, Brute force */
         /* allocate for vertices labels */
         L = (float *) SUMA_calloc (SO->N_Node, sizeof (float));
         if (!L) {
            fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
            SUMA_free(DC);
            SUMA_RETURN (NULL);
         }

         /* label all vertices with very large numbers, initialize path previous pointers to null */
         for (i=0; i < SO->N_Node; ++i) {
            L[i] = LARGE_NUM;   
            DC[i].Previous = NULL;
         }
         /* label starting vertex with 0 */
         L[Nx] = 0.0;
         Lmin = 0.0;
         v = Nx;
         *Lfinal = -1.0;
         /* initialize path at Nx */
         DC[Nx].Previous = NULL;
         DC[Nx].node = Nx;
         DC[Nx].le = 0.0;
         DC[Nx].order = 0;
         *N_Path = 0;
         /* Brute force method */
         do {
            /* find v in Mesh / L(v) is minimal */
            /* this sucks up a lot of time because it is searching the entire set of SO->N_Node instead of the one that was intersected only.
            This can be sped up, considerably */
            SUMA_MIN_LOC_VEC(L, SO->N_Node, Lmin, v);   /* locates and finds the minimum of L, nodes not in mesh will keep their large values and will not be picked*/
            if (!isNodeInMesh[v]) {
               fprintf (SUMA_STDERR, "\aERROR %s: Dijkstra derailed. v = %d, Lmin = %f\n. Try another point.", FuncName, v, Lmin);
               SUMA_free (L);
               SUMA_free(DC);
               SUMA_RETURN (NULL); 
            }
            if (v == Ny) {
               if (LocalHead) fprintf (SUMA_STDERR, "%s: Done.\n", FuncName);
               *Lfinal = L[v];
               Found = YUP;
            } else {
               N_Neighb = SO->FN->N_Neighb[v];
               for (i=0; i < N_Neighb; ++i) {
                  w = SO->FN->FirstNeighb[v][i];
                  if (isNodeInMesh[w]) {
                     iw = 3*w;
                     iv = 3*v;
                     le = sqrt ( (SO->NodeList[iw] - SO->NodeList[iv]) * (SO->NodeList[iw] - SO->NodeList[iv]) +
                                 (SO->NodeList[iw+1] - SO->NodeList[iv+1]) * (SO->NodeList[iw+1] - SO->NodeList[iv+1]) +
                                 (SO->NodeList[iw+2] - SO->NodeList[iv+2]) * (SO->NodeList[iw+2] - SO->NodeList[iv+2]) );
                     if (L[w] > L[v] + le ) {
                        L[w] = L[v] + le;  
                        /* update the path */
                        DCp = &(DC[v]); /* previous path */
                        DC[w].Previous = (void *) DCp;
                        DC[w].le = le;
                        DC[w].node = w;
                        DC[w].order = DCp->order + 1;
                     } 
                  }
               }

               /* remove node v from isNodeInMesh and reset their distance value to a very large one, 
                  this way you do not have to reinitialize this variable. */
               isNodeInMesh[v] = NOPE;
               *N_isNodeInMesh -= 1;
               L[v] = LARGE_NUM; 
               Found = NOPE;
            }
         } while (*N_isNodeInMesh > 0 && !Found);

         if (!Found) {
            fprintf (SUMA_STDERR, "Error %s: No more nodes in mesh, failed to reach target.\n", FuncName);
            SUMA_free (L);
            SUMA_free(DC);
            SUMA_RETURN (NULL);
         }else {
            if (LocalHead) fprintf (SUMA_STDERR, "%s: Path between Nodes %d and %d is %f.\n", FuncName, Nx, Ny, *Lfinal);
         }


         if (LocalHead) {
            /* stop timer */
            DT_DIJKSTRA = SUMA_etime(&start_time,1);
            fprintf (SUMA_STDERR, "%s: Method 1- Elapsed time in function %f seconds.\n", FuncName, DT_DIJKSTRA);
         }

         SUMA_free(L);
         break;

      case 1:  /********* Method 1- faster minimum searching *******************/
         if (LocalHead) {
            /* Start timer for next function */
            SUMA_etime(&start_time,0);      
         }

         /* allocate for vertices labels and minimums vectors*/
         L = (float *) SUMA_calloc (SO->N_Node, sizeof (float));        /* L[i] = distance to a node i*/
         Lmins = (float *) SUMA_calloc (SO->N_Node, sizeof (float));    /* Lmins = vector containing minimum calculated distances to node */
         vLmins = (int *) SUMA_calloc (SO->N_Node, sizeof (int));       /* vLmins[i] = index (into L) of the node having a distance Lmins[i] */
         vLocInLmins = (int *) SUMA_calloc (SO->N_Node, sizeof (int));  /* vLocInLmin[j] = index (into Lmins) of a node having index j (into L) */

         if (!L || !Lmins || !vLmins || !vLocInLmins) {
            fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
            SUMA_RETURN (NULL);
         }

         /* label all vertices with very large numbers and initialize vLocInLmins to -1*/
         for (i=0; i < SO->N_Node; ++i) {
            L[i] = LARGE_NUM;
            Lmins[i] = LARGE_NUM;   
            vLocInLmins[i] = -1;            
            DC[i].Previous = NULL;
         }

         /* label starting vertex with 0 */
         L[Nx] = 0.0;
         *Lfinal = -1.0;

         /* initialize values of vectors used to keep track of minimum values of L and their corresponding nodes */
         Lmins[0] = 0.0;
         vLmins[0] = Nx;
         vLocInLmins[Nx] = 0;
         N_Lmins = 1;

         /* initialize path at Nx */
         DC[Nx].Previous = NULL;
         DC[Nx].node = Nx;
         DC[Nx].le = 0.0;
         DC[Nx].order = 0;
         *N_Path = 0;
         
         /* method with efficient tracking of minimum */
         if (LocalHead) fprintf (SUMA_STDERR, "%s: about to MIN_LOC ....N_isNodeInMesh = %d\n", FuncName, *N_isNodeInMesh);
         do {
            /* find v in Mesh / L(v) is minimal */
            SUMA_MIN_LOC_VEC(Lmins, N_Lmins, Lmin, iLmins);   /* locates the minimum value in Lmins vector */
            v = vLmins[iLmins];   /* get the node for this Lmin value */
            if (!isNodeInMesh[v]) {
               fprintf (SUMA_STDERR, "\aERROR %s: Dijkstra derailed. v = %d, Lmin = %f\n. Try another point.", FuncName, v, Lmin);
               SUMA_free (L);
               SUMA_free (Lmins);
               SUMA_free(vLmins);
               SUMA_free(vLocInLmins);
               SUMA_free(DC);
               SUMA_RETURN (NULL);
            }
            #ifdef LOCALDEBUG
               fprintf (SUMA_STDERR, "%s: Node v = %d.\n", FuncName, v);
            #endif
            if (v == Ny) {
               if (LocalHead) fprintf (SUMA_STDERR, "%s: Done.\n", FuncName);
               *Lfinal = L[v];
               Found = YUP;
            } else {
               N_Neighb = SO->FN->N_Neighb[v];
               for (i=0; i < N_Neighb; ++i) {
                  w = SO->FN->FirstNeighb[v][i];
                  if (isNodeInMesh[w]) {
                     iw = 3*w;
                     iv = 3*v;
                     le = sqrt ( (SO->NodeList[iw] - SO->NodeList[iv]) * (SO->NodeList[iw] - SO->NodeList[iv]) +
                                 (SO->NodeList[iw+1] - SO->NodeList[iv+1]) * (SO->NodeList[iw+1] - SO->NodeList[iv+1]) +
                                 (SO->NodeList[iw+2] - SO->NodeList[iv+2]) * (SO->NodeList[iw+2] - SO->NodeList[iv+2]) );
                     if (L[w] > L[v] + le ) {
                        #ifdef LOCALDEBUG
                           fprintf (SUMA_STDERR, "%s: L[%d]=%f > L[%d] = %f + le = %f.\n", FuncName, w, L[w], v, L[v], le);
                        #endif
                        L[w] = L[v] + le; 
                        /* update the path */
                        DCp = &(DC[v]); /* previous path */
                        DC[w].Previous = (void *) DCp;
                        DC[w].le = le;
                        DC[w].node = w;
                        DC[w].order = DCp->order + 1;
                        
                        if (vLocInLmins[w] < 0) { 
                           #ifdef LOCALDEBUG
                              fprintf (SUMA_STDERR, "%s: adding entry for w = %d - First Hit. \n", FuncName, w);
                           #endif
                           Lmins[N_Lmins] = L[w]; /* add this value to Lmins vector */
                           vLmins[N_Lmins] = w; /* store the node for this Lmins value */
                           vLocInLmins[w] = N_Lmins; /* store where that node is represented in Lmins */
                           ++N_Lmins;  /* increment N_Lmins */  
                        } else {
                           #ifdef LOCALDEBUG
                              fprintf (SUMA_STDERR, "%s: modifying entry for w = %d  Second Hit.\n", FuncName, w); */
                           #endif
                           Lmins[vLocInLmins[w]] = L[w]; /* update value for Lmins */
                        }                        
                     }else {
                        #ifdef LOCALDEBUG
                           fprintf (SUMA_STDERR, "%s: L[%d]=%f < L[%d] = %f + le = %f.\n", FuncName, w, L[w], v, L[v], le); */
                        #endif
                     } 
                  }
               }

               /* remove node v from isNodeInMesh and reset their distance value to a very large one, 
                  this way you do not have to reinitialize this variable. */
               isNodeInMesh[v] = NOPE;
               *N_isNodeInMesh -= 1;
               L[v] = LARGE_NUM; 
               Found = NOPE;

               /* also remove the values (by swapping it with last element) for this node from Lmins */
               #ifdef LOCALDEBUG
                  {
                     int kkk;
                     fprintf (SUMA_STDERR,"Lmins\tvLmins\tvLocInLmins\n");
                     for (kkk=0; kkk < N_Lmins; ++kkk) fprintf (SUMA_STDERR,"%f\t%d\t%d\n", Lmins[kkk], vLmins[kkk], vLocInLmins[vLmins[kkk]] );
               
                  }
               #endif
               
               if (vLocInLmins[v] >= 0) { /* remove its entry if there is one */
                  #ifdef LOCALDEBUG
                     fprintf (SUMA_STDERR, "%s: removing node v = %d. N_Lmins = %d\n", FuncName,  v, N_Lmins);
                  #endif
                  --N_Lmins;
                  ReplacingNode = vLmins[N_Lmins];
                  ReplacedNodeLocation = vLocInLmins[v];
                  Lmins[vLocInLmins[v]] = Lmins[N_Lmins];
                  vLmins[vLocInLmins[v]] = vLmins[N_Lmins];
                  vLocInLmins[ReplacingNode] = ReplacedNodeLocation;
                  vLocInLmins[v] = -1;
                  Lmins[N_Lmins] = LARGE_NUM; 
               }
            }
         } while (*N_isNodeInMesh > 0 && !Found);

         if (!Found) {
            fprintf (SUMA_STDERR, "Error %s: No more nodes in mesh, failed to reach target %d. NLmins = %d\n", FuncName, Ny, N_Lmins);
            SUMA_free (L);
            SUMA_free (Lmins);
            SUMA_free(vLmins);
            SUMA_free(vLocInLmins);
            SUMA_free(DC);
            SUMA_RETURN (NULL);
         }else {
            if (LocalHead) fprintf (SUMA_STDERR, "%s: Path between Nodes %d and %d is %f.\n", FuncName, Nx, Ny, *Lfinal);
         }


         if (LocalHead) {
            /* stop timer */
            DT_DIJKSTRA = SUMA_etime(&start_time,1);
            fprintf (SUMA_STDERR, "%s: Method 2- Elapsed time in function %f seconds.\n", FuncName, DT_DIJKSTRA);
         }

         SUMA_free(L);
         SUMA_free(Lmins);
         SUMA_free(vLmins);
         SUMA_free(vLocInLmins);
         break;   /********** Method 1- faster minimum searching **************/
      default: 
         fprintf (SUMA_STDERR, "Error %s: No such method (%d).\n", FuncName, Method_Number);
         if (DC) SUMA_free(DC);
         SUMA_RETURN (NULL);
         break;
   }
   
   /* now reconstruct the path */
   *N_Path = DC[Ny].order+1;
   Path = (int *) SUMA_calloc (*N_Path, sizeof(int));
   if (!Path) {
      fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
      if (DC) SUMA_free(DC);
      SUMA_RETURN (NULL);
   }
   
   DCi = &(DC[Ny]);
   iv = *N_Path - 1;
   Path[iv] = Ny;
   if (iv > 0) {
      do {
         --iv;
         DCp = (SUMA_DIJKSTRA_PATH_CHAIN *) DCi->Previous;
         Path[iv] = DCp->node;
         DCi = DCp;
      } while (DCi->Previous);
   }
   
   if (iv != 0) {
      fprintf (SUMA_STDERR, "Error %s: iv = %d. This should not be.\n", FuncName, iv);
   }  
   
   SUMA_free(DC);
   SUMA_RETURN (Path);
}

/*!
\brief Converts a path formed by a series of connected nodes to a series of edges
   ePath = SUMA_NodePath_to_EdgePath (EL, Path, N_Path, N_Edge);
   \param EL (SUMA_EDGE_LIST *) Pointer to edge list structure
   \param Path (int *) vector of node indices forming a path.
                       Sequential nodes in Path must be connected on the surface mesh.
   \param N_Path (int) number of nodes in the path 
   \param N_Edge (int *) pointer to integer that will contain the number of edges in the path
                        usually equal to N_Path if path is a loop (closed) or N_Path - 1. 
                        0 if function fails.
   \param ePath (int *) pointer to vector containing indices of edges forming the path. 
                        The indices are into EL->EL and represent the first occurence of the
                        edge between Path[i] and Path[i+1].
                        NULL if trouble is encountered.
                        
   \sa SUMA_NodePath_to_EdgePath_Inters
   \sa Path S in labbook NIH-2 page 153
*/
int *SUMA_NodePath_to_EdgePath (SUMA_EDGE_LIST *EL, int *Path, int N_Path, int *N_Edge)
{
   static char FuncName[]={"SUMA_NodePath_to_EdgePath"};
   int *ePath = NULL, i, i0;
   SUMA_Boolean LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
 
 
  *N_Edge = 0;
   ePath = (int *) SUMA_calloc(N_Path, sizeof(int));
   if (!ePath) {
      fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
      SUMA_RETURN (NULL);
   }

   for (i=1; i<N_Path; ++i) {
      i0 = Path[i-1];
      /* find the location of the edge between i0 and i1 */
      ePath[i-1] = SUMA_FindEdge (EL, i0, Path[i]); 
      if (ePath[i-1] < 0) { /* error */
         fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_FindEdge.\n", FuncName);
         SUMA_free(ePath);
         *N_Edge = 0;
         SUMA_RETURN (NULL);   
      }else {
         ++(*N_Edge);
      }
   }

   SUMA_RETURN (ePath);   
}   

/*!
\brief determines whether to edges are identical or not. Recall
that an edge can be represented multiple times in SO->EL, once for
each triangle that uses it. Two edges are the same if and only if
EL->EL[E1][0] == EL->EL[E2][0] && EL->EL[E1][1] == EL->EL[E2][1]

ans = SUMA_isSameEdge ( EL, E1, E2); 

\param EL (SUMA_EDGE_LIST *) Edge List structure.
\param E1 (int) edge index
\param E2 (int) edge index
\return ans (SUMA_Boolean) YUP/NOPE
*/

SUMA_Boolean SUMA_isSameEdge (SUMA_EDGE_LIST *EL, int E1, int E2) 
{
   static char FuncName[]={"SUMA_isSameEdge"};
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   if (EL->EL[E1][0] == EL->EL[E2][0] && EL->EL[E1][1] == EL->EL[E2][1]) {
      SUMA_RETURN (YUP);
   } else {
      SUMA_RETURN (NOPE);
   }
   
}

/*!
\brief This function determines the strip of triangles necessary to go from one node to another
along intersected edges. 
tPath = SUMA_IntersectionStrip (SO, SPI, 
            int *nPath, int N_nPath, float *dinters, float dmax, int *N_tPath)
\param SO (SUMA_SurfaceObject *) pointer to Surface Object structure
\param SPI (SUMA_SURF_PLANE_INTERSECT *) pointer surface/plane intersection structure
\param nPath (int *) series of nodes forming the Dijkstra path between nodes Nx (nPath[0] and NynPath[N_nPath-1])
\param N_nPath (int) number of nodes in nPath. Note: Only nPath[0], nPath[1] and nPath[N_nPath-1] are used by this function
\param dinters (float *) pointer sum of distances between intersection points on intersected edges for all triangles in tPath.
               This distance is a better approximation for the distance along the cortical surface than the distance obtained
               along the shortest path.
\param dmax (float) distance beyond which to quit searching. Usually this distance is slightly larger than the distance
                  along the path returned by SUMA_Dijkstra but dinters should always be less than the distance along the shortest path.
\param N_tPath (int *) pointer to the number of triangle indices in tPath
\return tPath (int*) pointer to vector containing the indices of triangles travelled from 
         nPath[0] to nPath[N_nPath] (or vice versa if nPath[0] = SO->N_Node-1).

NOTE: Although the number of elements in tPath may be small, the number of elements allocated for is SO->N_FaceSet
Make sure you free tPath when you are done with it.

NOTE: This function can be used to create a node path formed by intersection points along edges but that is not implemented yet.

\sa SUMA_Surf_Plane_Intersect
\sa SUMA_Dijkstra
\sa SUMA_FromIntEdgeToIntEdge

*/
int * SUMA_IntersectionStrip (SUMA_SurfaceObject *SO, SUMA_SURF_PLANE_INTERSECT *SPI, 
            int *nPath, int N_nPath, float *dinters, float dmax, int *N_tPath)
{
   static char FuncName[]={"SUMA_IntersectionStrip"};
   int *tPath1 = NULL, *tPath2 = NULL, Incident[50], N_Incident, Nx = -1, 
      Ny = -1, Tri = -1, Tri1 = -1, istart, n2 = -1, n3 = -1, E1, E2, cnt, N_tPath1, N_tPath2;
   float d1, d2;
   SUMA_Boolean *Visited = NULL, Found, LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   /* find the edge containing the 1st 2 nodes of the Dijkstra path */
   /* find the triangle that contains the edge formed by the 1st 2 nodes of the Dijkstra path and is intersected by the plane*/
   Tri1 = -1;
   if (LocalHead) {
      fprintf (SUMA_STDERR, "%s: Looking for a triangle containing nodes [%d %d].\n", FuncName, nPath[0], nPath[1]);
   }
   
   Found = SUMA_Get_Incident(nPath[0], nPath[1], SO->EL, Incident, &N_Incident);
   if (!Found) {
      /* no such triangle, get a triangle that contains nPath[0] and is intersected */
      fprintf (SUMA_STDERR, "%s: No triangle contains nodes [%d %d].\n", FuncName, nPath[0], nPath[1]);
      if (nPath[0] == SO->N_Node - 1) {
         fprintf (SUMA_STDERR, "Warning %s: 1st node is last node of surface, traversing path backwards.\n", FuncName);
         Nx = nPath[N_nPath - 1];
         Ny = nPath[0];
      }else {
         Nx = nPath[0];
         Ny = nPath[N_nPath - 1];
      }
      istart = SO->EL->ELloc[Nx];
      /* find an edge containing the first node and belonging to an intersected triangle */
      Found = NOPE;
      while (SO->EL->EL[istart][0] == Nx && !Found) {
         Tri = SO->EL->ELps[istart][1];
         if (SPI->isTriHit[Tri]) {
            Found = YUP;
            Tri1 = Tri;
         }
         ++istart;
      } 
   }else {
      Nx = nPath[0];
      Ny = nPath[N_nPath - 1];
      
      /* find which of these triangles was intersected */
      if (LocalHead) {
         fprintf (SUMA_STDERR, "%s: Found %d triangles containing nodes [%d %d].\n", FuncName, N_Incident, nPath[0], nPath[1]);
         for (cnt = 0; cnt < N_Incident; ++cnt) fprintf (SUMA_STDERR, "%d isHit %d\n", Incident[cnt], SPI->isTriHit[Incident[cnt]]);
         fprintf (SUMA_STDERR, "\n"); 
      }
      Found = NOPE;
      cnt = 0;
      while (cnt < N_Incident && !Found) {
         if (SPI->isTriHit[Incident[cnt]]) {
            Found = YUP;
            Tri1 = Incident[cnt];
         }
         ++cnt;
      }
   }
   
   if (!Found) {
      fprintf (SUMA_STDERR, "Error %s: Starting Edge could not be found.\n", FuncName);
      SUMA_RETURN (NULL);
   }else if (LocalHead) {
      fprintf (SUMA_STDERR, "%s: Starting with triangle %d.\n", FuncName, Tri1);
   }

   /* found starting triangle edge, begin with side 1 */
   if (SO->FaceSetList[3*Tri1] == Nx) {
      n2 = SO->FaceSetList[3*Tri1+1];
      n3 = SO->FaceSetList[3*Tri1+2];
   } else if (SO->FaceSetList[3*Tri1+1] == Nx) {
      n2 = SO->FaceSetList[3*Tri1];
      n3 = SO->FaceSetList[3*Tri1+2];
   } else if (SO->FaceSetList[3*Tri1+2] == Nx) {
      n2 = SO->FaceSetList[3*Tri1];
      n3 = SO->FaceSetList[3*Tri1+1];
   } else {
      fprintf (SUMA_STDERR, "Error %s: Triangle %d does not contain Nx %d.\n", FuncName, Tri1, Nx);
      SUMA_RETURN (NULL);
   }  
   
   
   
   E1 = SUMA_FindEdgeInTri (SO->EL, Nx, n2, Tri1);
   if (!SPI->isEdgeInters[E1]) {
      E1 = SUMA_FindEdgeInTri (SO->EL, Nx, n3, Tri1);
   }
   /* now choose E2 such that E2 is also intersected */
   if (!SUMA_isSameEdge (SO->EL, SO->EL->Tri_limb[Tri1][0], E1) && SPI->isEdgeInters[SO->EL->Tri_limb[Tri1][0]]) {
      E2 = SO->EL->Tri_limb[Tri1][0];
   }else if (!SUMA_isSameEdge (SO->EL, SO->EL->Tri_limb[Tri1][1], E1) && SPI->isEdgeInters[SO->EL->Tri_limb[Tri1][1]]) {
      E2 = SO->EL->Tri_limb[Tri1][1];
   }else if (!SUMA_isSameEdge (SO->EL, SO->EL->Tri_limb[Tri1][2], E1) && SPI->isEdgeInters[SO->EL->Tri_limb[Tri1][2]]) {
      E2 = SO->EL->Tri_limb[Tri1][2];
   }else {
      fprintf (SUMA_STDERR,"Error %s: No E2 found.\n", FuncName);
      SUMA_RETURN (NULL);
   }

   Visited = (SUMA_Boolean *) SUMA_calloc (SO->N_FaceSet, sizeof(SUMA_Boolean));
   tPath1 = (int *) SUMA_calloc (SO->N_FaceSet, sizeof(int));
   if (!Visited || !tPath1) {
      fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
      if (Visited) SUMA_free(Visited);
      if (tPath2) SUMA_free(tPath1); 
      SUMA_RETURN (NULL);
   }
   
   N_tPath1 = 0;
   if (!SUMA_FromIntEdgeToIntEdge (Tri1, E1, E2, SO->EL, SPI, Ny, Visited, &d1, dmax, tPath1, &N_tPath1)) {
      fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_FromIntEdgeToIntEdge.\n", FuncName);
      if (Visited) SUMA_free(Visited);
      if (tPath2) SUMA_free(tPath1);      
      SUMA_RETURN (NULL);
   }
   
   if (LocalHead) {
      fprintf (SUMA_STDERR, "%s: Found a distance of %f.\n\n\n", FuncName, d1);
   }
   
   /* Now try going in the other direction, E2->E1 */
   cnt = E2;
   E2 = E1;
   E1 = cnt;
   
   /* reset the values of Visited */
   for (cnt=0; cnt < SO->N_FaceSet; ++cnt) if (Visited[cnt]) Visited[cnt] = NOPE;
   
   tPath2 = (int *) SUMA_calloc (SO->N_FaceSet, sizeof(int));
   if (!Visited || !tPath2) {
      fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
      if (Visited) SUMA_free(Visited);
      if (tPath1) SUMA_free(tPath1);
      if (tPath2) SUMA_free(tPath2);
      SUMA_RETURN (NULL);
   }

   N_tPath2 = 0;
   if (!SUMA_FromIntEdgeToIntEdge (Tri1, E1, E2, SO->EL, SPI, Ny, Visited, &d2, dmax, tPath2, &N_tPath2)) {
      fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_FromIntEdgeToIntEdge.\n", FuncName);
      if (Visited) SUMA_free(Visited);
      if (tPath1) SUMA_free(tPath1);
      if (tPath2) SUMA_free(tPath2);
      SUMA_RETURN (NULL);
   }
   
   if (Visited) SUMA_free(Visited);
   
   if (LocalHead) {
      fprintf (SUMA_STDERR, "%s: Found a distance of %f.\n", FuncName, d2);
   }
   
   if (d2 < d1) {
      *N_tPath = N_tPath2;
      *dinters = d2;
      if (tPath1) SUMA_free(tPath1);
      SUMA_RETURN (tPath2);
   } else {
      *dinters = d1;
      *N_tPath = N_tPath1;
      if (tPath2) SUMA_free(tPath2);
      SUMA_RETURN (tPath1);
   }
   
}

/*!
\brief This function moves from one intersected edge to the next until a certain node is encountered or a 
a certain distance is exceeded. By intersected edge, I mean an edge of the surface's mesh that was 
intersected by a plane.

ans = SUMA_FromIntEdgeToIntEdge (int Tri, int E1, int E2, SUMA_EDGE_LIST *EL, SPI, int Ny,
         Visited, float *d, float dmax, int *tPath, int *N_tPath);

\param Tri (int) index of triangle to start with (index into SO->FaceSetList)
\param E1 (int) index of edge in Tri to start from
\param E2 (int) index of edge in Tri to move in the direction of (Both E1 and E2  must be intersected edges)
\param EL (SUMA_EDGE_LIST *) pointer to the edge list structure, typically SO->EL
\param SPI (SUMA_SURF_PLANE_INTERSECT *) pointer to structure containing intersection of plane with surface
\param Ny (int) node index to stop at (index into SO->NodeList)
\param Visited (SUMA_Boolean *) pointer to vector (SO->N_FaceSet elements) that keeps track of triangles visited.
      This vector should be all NOPE when you first call this function. 
\param d (float *) pointer to total distance from first intersected edge E1 to the last edge that contains E2
\param dmax (float) maximum distance to go for before reversing and going in the other direction. Typically 
         this measure should be a bit larger than the distance of a Dijkstra path although you should never get a 
         distance that is larger than the Dijkstra path.
\param tPath (int *) vector of indices of triangles visited from first edge to last edge (make sure you allocate a bundle for tPath)
\param N_tPath (int *) number of elements in tPath
\return ans (SUMA_Boolean) YUP/NOPE, for success/failure. 

         NOTE: This function is recursive.

\sa SUMA_Surf_Plane_Intersect
\sa SUMA_IntersectionStrip

*/
SUMA_Boolean SUMA_FromIntEdgeToIntEdge (int Tri, int E1, int E2, SUMA_EDGE_LIST *EL, SUMA_SURF_PLANE_INTERSECT *SPI, int Ny,
         SUMA_Boolean *Visited, float *d, float dmax, int *tPath, int *N_tPath)
{  static char FuncName[]={"SUMA_FromIntEdgeToIntEdge"};
   int Tri2 = 0, cnt, Incident[5], N_Incident;
   float dx, dy, dz;
   SUMA_Boolean Found, LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

   if (Tri < 0 || E1 < 0 || E2 < 0) {
      fprintf (SUMA_STDERR, "Error %s: Tri (%d) or E1 (%d) or E2 (%d) is negative!\n", FuncName, Tri, E1, E2);
      SUMA_RETURN (NOPE);
   }
   
   
   dx = (SPI->IntersNodes[3*E2] - SPI->IntersNodes[3*E1]);
   dy = (SPI->IntersNodes[3*E2+1] - SPI->IntersNodes[3*E1+1]);
   dz = (SPI->IntersNodes[3*E2+2] - SPI->IntersNodes[3*E1+2]);
   if (LocalHead) {
      fprintf (SUMA_STDERR, "%s: Entered - Tri %d, E1 %d [%d %d], E2 %d [%d %d]\n\tdx = %f dy = %f dz = %f\n", 
         FuncName, Tri, E1, EL->EL[E1][0], EL->EL[E1][1], E2, EL->EL[E2][0], EL->EL[E2][1], dx, dy, dz);
   }
   *d += sqrt( dx * dx + dy * dy + dz * dz);
   
   if (*d > dmax) {
      /* path already longer than Dijkstra path, no need to search further in this direction, get out with this d value */
      fprintf (SUMA_STDERR, "%s: Path longer than dmax. Returning.\n", FuncName);
      SUMA_RETURN (YUP);
   }
   
   if (EL->EL[E2][0] == Ny || EL->EL[E2][1] == Ny) {
      fprintf (SUMA_STDERR, "%s: Found Ny, d = %f\n", FuncName, *d);
      if (!Visited[Tri]) {
         /* add triangle to path */
         tPath[*N_tPath] = Tri;
         ++*N_tPath;
      }
      SUMA_RETURN (YUP);
   } else if (Visited[Tri]) {
      fprintf (SUMA_STDERR, "Error %s: Triangle %d already visited.\n",FuncName, Tri); 
      SUMA_RETURN (NOPE);
   }
     
   /* mark triangle as visited */
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Marking triangle %d and adding %dth element to tPath.\n", FuncName, Tri, *N_tPath);
   Visited[Tri] = YUP;
   
   /* add triangle to path */
   tPath[*N_tPath] = Tri;
   ++*N_tPath;
   
   /* now get the second intersected triangle, incident to E2 */
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Searching for triangles incident to E2 %d.\n", FuncName, E2);
   if (!SUMA_Get_Incident(EL->EL[E2][0], EL->EL[E2][1], EL, Incident, &N_Incident)) {
      fprintf (SUMA_STDERR,"Error %s: Failed to get Incident triangles.\n", FuncName);
      SUMA_RETURN (NOPE);
   }
   
   /* find Tri2 such that Tri2 != Tri and Tri2 is an intersected triangle */
   cnt = 0;
   Found = NOPE;
   while (cnt < N_Incident && !Found) {
      if (SPI->isTriHit[Incident[cnt]] && Incident[cnt] != Tri && !Visited[Incident[cnt]]) {
         Found = YUP;
         Tri2 = Incident[cnt];
      }
      ++cnt;
   }
   
   if (!Found) {
      fprintf (SUMA_STDERR,"Error %s: Could not find next triangle.\n", FuncName);
      SUMA_RETURN (NOPE);
   }
   
   Tri = Tri2;
   E1 = E2;
   
   /* now find the new E2 */
   if (LocalHead) fprintf (SUMA_STDERR, "%s: Finding new E2.\n", FuncName);
   
   if (!SUMA_isSameEdge (EL, EL->Tri_limb[Tri][0], E1) && SPI->isEdgeInters[EL->Tri_limb[Tri][0]]) {
      E2 = EL->Tri_limb[Tri][0];
   }else if (!SUMA_isSameEdge (EL, EL->Tri_limb[Tri][1], E1) && SPI->isEdgeInters[EL->Tri_limb[Tri][1]]) {
      E2 = EL->Tri_limb[Tri][1];
   }else if (!SUMA_isSameEdge (EL, EL->Tri_limb[Tri][2], E1) && SPI->isEdgeInters[EL->Tri_limb[Tri][2]]) {
      E2 = EL->Tri_limb[Tri][2];
   }else {
      fprintf (SUMA_STDERR,"Error %s: No E2 found.\n", FuncName);
      SUMA_RETURN (NOPE);
   }
   
   /* call the same function again */
   if (!SUMA_FromIntEdgeToIntEdge (Tri, E1, E2, EL, SPI, Ny, Visited, d, dmax, tPath, N_tPath)) {
      fprintf (SUMA_STDERR,"Error %s: Failed in SUMA_FromIntEdgeToIntEdge.\n", FuncName);
      SUMA_RETURN (NOPE);
   }
   
   SUMA_RETURN (YUP);
}
/*!
\brief Converts a series of connected nodes into a series of connected triangles that were intersected by 
the plane. 
There is no guarantee that two nodes that belong to triangles intersected by the plane and part of the shortest path
(as returned by SUMA_Dijkstra) for an edge that belongs to a triangle intersected by the plane. See labbook NIH-2 page 
158 for sketches illustrating this point. So the strip of triangles that you will get back may have holes in it since 
it only allows intersected triangles to be members of the path. 

   tPath = SUMA_NodePath_to_TriPath_Inters (SO, SPI, int *nPath, int N_nPath, int *N_tPath)
   
   \param SO (SUMA_SurfaceObject *) structure containing surface object
   \param SPI (SUMA_SURF_PLANE_INTERSECT *) surface plane intersection structure
   \param nPath (int *) vector containing the shortest path defined by its nodes (output of SUMA_Dijkstra)
   \param N_nPath (int) number of elements in nPath
   \param N_tPath (int *)pointer to number of elements returned in tPath
   \return tPath (int *) vector of *N_tPath indices of triangles that form a strip along the shortest path.
   
   \sa SUMA_Dijkstra
   \sa SUMA_NodePath_to_EdgePath
   \sa SUMA_Surf_Plane_Intersect
   
   \sa labbook NIH-2 pages 158, and 159 for MissingTriangles
*/

int *SUMA_NodePath_to_TriPath_Inters ( SUMA_SurfaceObject *SO, SUMA_SURF_PLANE_INTERSECT *SPI, int *nPath, int N_nPath, int *N_tPath)
{
   static char FuncName[]={"SUMA_NodePath_to_TriPath_Inters"};
   int *tPath = NULL, e, i, N_nc, nc[3], N_HostTri, E, j, 
      HostTri, PrevTri, k, N1[2], N2[2], cnt, MissTri = 0, candidate;
   SUMA_Boolean Found, LocalHead = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
   
   tPath = (int *) SUMA_calloc(2*N_nPath, sizeof(int));
   if (!tPath) {
      fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
      SUMA_RETURN (NULL);
   }
   
   *N_tPath = 0;
   for (i=0; i < N_nPath - 1; ++i) {
      /* find the edge corresponding to two consecutive nodes in the path */
      E = SUMA_FindEdge (SO->EL, nPath[i], nPath[i+1]);
      if (E < 0) {
         fprintf (SUMA_STDERR, "Error %s: Failed in SUMA_FindEdge.\n", FuncName);
         SUMA_free(tPath);
         SUMA_RETURN(NULL);
      }
      /* find the triangles containing E and intersected by the plane */
      N_HostTri = SO->EL->ELps[E][2]; /* number of hosting triangles */
      if (N_HostTri > 2) {
         fprintf (SUMA_STDERR, "Warning %s: Surface is not a surface, Edge %d has more than %d hosting triangles.\n", FuncName, E, N_HostTri);
      }
      candidate = 0;
      /* search for a hosting triangle that was intersected */
      for (j=0; j < N_HostTri; ++j) {
         HostTri = SO->EL->ELps[E+j][1];
         if (SPI->isTriHit[HostTri]) { /* a candidate for adding to the path */
            ++candidate;
            if (*N_tPath > 2*N_nPath) {
               fprintf (SUMA_STDERR, "Error %s: N_tPath = %d > %d allocated.\n", FuncName, *N_tPath, 2*N_nPath);
            }  
            #if 1
            /* This block is an attempt to get All triangles intersected by plane AND having a node as part of the shortest path.
            It does not work well, probably because some of the functions called need fixing... */
            if (*N_tPath == 0) { /* if that is the first triangle in the path, add it without much fuss */
               tPath[*N_tPath] = HostTri; /* hosting triangle index */
               ++ (*N_tPath);
            } else { /* make sure there is continuation along edges */
               PrevTri = tPath[*N_tPath - 1];
               N_nc = SUMA_isTriLinked (&(SO->FaceSetList[3*PrevTri]), &(SO->FaceSetList[3*HostTri]), nc);
               if (!N_nc) {
                  fprintf (SUMA_STDERR, "Warning %s: Triangles %d and %d are not linked.\nAdding triangle %d anyway.\n", 
                     FuncName, PrevTri, HostTri, HostTri);
                  /* add triangle, anyway */
                  tPath[*N_tPath] = HostTri; 
                  ++ (*N_tPath);
               }else if (N_nc == 1) {
                  /* must fill triangle gap get the triangle with the common node and common edges*/
                  /* first, find remaining nodes in PrevTri  */
                  e = 0;
                  for (k=0; k <3; ++k) {
                     if (SO->FaceSetList[3*PrevTri+k] != nc[0]) {
                        N1[e] = SO->FaceSetList[3*PrevTri+k]; ++e;
                     }
                  }
                  /* then find remaining nodes in HostTri  */
                  e = 0;
                  for (k=0; k <3; ++k) {
                     if (SO->FaceSetList[3*HostTri+k] != nc[0]) {
                        N2[e] = SO->FaceSetList[3*HostTri+k]; ++e;
                     }
                  }
                  /* find a triangle that has either one of the following node combinations, in addition to nc[0]:
                  N1[0], N2[0] or N1[0], N2[1] or N1[1], N2[0] or N1[1], N2[1] */
                  Found = NOPE;
                  cnt = 0;
                  while (!Found && cnt < 4) {
                     switch (cnt) {
                        case 0:
                           MissTri = SUMA_whichTri (SO->EL, nc[0], N1[0], N2[0]);
                           if (LocalHead) fprintf (SUMA_STDERR, "%s: looking for triangle with nodes %d and %d... Tri = %d\n", 
                                 FuncName, N1[0], N2[0], MissTri);
                           break;
                        case 1:
                           MissTri = SUMA_whichTri (SO->EL, nc[0], N1[0], N2[1]);
                           if (LocalHead) fprintf (SUMA_STDERR, "%s: looking for triangle with nodes %d and %d... Tri = %d\n", 
                                 FuncName, N1[0], N2[1], MissTri);
                           break;
                        case 2:
                           MissTri = SUMA_whichTri (SO->EL, nc[0], N1[1], N2[0]);
                           if (LocalHead) fprintf (SUMA_STDERR, "%s: looking for triangle with nodes %d and %d... Tri = %d\n", 
                                 FuncName, N1[1], N2[0], MissTri);
                           break;
                        case 3:
                           MissTri = SUMA_whichTri (SO->EL, nc[0], N1[1], N2[1]);
                           if (LocalHead) fprintf (SUMA_STDERR, "%s: looking for triangle with nodes %d and %d... Tri = %d\n", 
                                 FuncName, N1[1], N2[1], MissTri);
                           break;
                     }
                     if (MissTri >= 0) {
                        Found = YUP;
                     }
                     ++cnt;
                  }
                  if (!Found) {
                     fprintf (SUMA_STDERR, "Warning %s: Failed to find missing triangle.\n", FuncName);
                     tPath[*N_tPath] = HostTri; 
                     ++ (*N_tPath);
                  }else {
                     /* add the missing triangle first, then the HostTri */
                     tPath[*N_tPath] = MissTri; 
                     ++ (*N_tPath);
                     tPath[*N_tPath] = HostTri; 
                     ++ (*N_tPath);
                  }
               }else if (N_nc == 2) {
                  /* Triangles share an edge so no problem, insert the new triangle in the path */
                  tPath[*N_tPath] = HostTri; 
                  ++ (*N_tPath);
               }else {
                  fprintf (SUMA_STDERR, "Error %s: Triangles %d and %d are identical.\n", FuncName, PrevTri, HostTri);
                  SUMA_free(tPath);
                  SUMA_RETURN(NULL);
               }
            }
            #else 
               tPath[*N_tPath] = HostTri; 
               ++ (*N_tPath);
            #endif   
         }
      }
      if (!candidate) {
         fprintf (SUMA_STDERR, "\aWarning %s: Nodes %d and %d of edge %d had no intersected hosting triangle.\n", FuncName, nPath[i], nPath[i+1], E);
         
      }
   }
   
   SUMA_RETURN (tPath);   
}
/*!
\brief Converts a series of connected nodes into a series of connected triangles that belong to a branch.
The function fails at times, picking the long instead of the short path but it is left here in case I 
need it in the future.
 
   tPath = SUMA_NodePath_to_TriPath_Inters_OLD (SO, Bv, Path, N_Path, N_Tri);
   \param SO (SUMA_SurfaceObject *) Pointer to surface object
   \param Bv (SUMA_TRI_BRANCH*) Pointer to tiangle branch containing nodes in path.
   \param Path (int *) vector of node indices forming a path.
                       Sequential nodes in Path must be connected on the surface mesh.
   \param N_Path (int) number of nodes in the path 
   \param N_Tri (int *) pointer to integer that will contain the number of triangles in the path
                        0 if function fails.
   \return tPath (int *) pointer to vector containing indices of triangles forming the path. 
                        The indices are into SO->FaceSetList.
                        NULL if trouble is encountered.
                        
   \sa SUMA_NodePath_to_EdgePath
   \sa Path I in NIH-2, labbook page 153
*/
int *SUMA_NodePath_to_TriPath_Inters_OLD (SUMA_SurfaceObject *SO, SUMA_TRI_BRANCH *Bv, int *Path, int N_Path, int *N_Tri)
{
   static char FuncName[]={"SUMA_NodePath_to_TriPath_Inters_OLD"};
   int *tPath = NULL, ilist, i0, Tri, eTri, EdgeBuf, Tri0, Tri1, Direction, i1, loc2f, iDirSet;
   SUMA_Boolean LocalHead = NOPE, Found = NOPE;
   
   if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);
 
 
  *N_Tri = 0;
   tPath = (int *) SUMA_calloc(Bv->N_list+1, sizeof(int));
   if (!tPath) {
      fprintf (SUMA_STDERR, "Error %s: Failed to allocate.\n", FuncName);
      SUMA_RETURN (NULL);
   }
   
   /* the first triangle should contain the first node in the path */
      i0 = Path[0];
      Tri0 = Bv->list[0];
      if (SO->FaceSetList[3*Tri0] != i0 && SO->FaceSetList[3*Tri0+1] != i0 && SO->FaceSetList[3*Tri0+2] != i0) {
         fprintf (SUMA_STDERR, "Error %s: Did not find node %d in first triangle in branch.\n", FuncName, i0);
         SUMA_free(tPath);
         *N_Tri = 0;
         SUMA_RETURN (NULL);  
      }
   
   
   /* initiliaze first node results and look for the second node */
   tPath[0] = Tri0;
   *N_Tri = 1;
   Found = NOPE;
   ilist = 0;

   if (LocalHead)   fprintf(SUMA_STDERR, "%s: Going forward looking for third node\n", FuncName);
   if (N_Path > 2) {
      iDirSet = 2; /* search for third node in list, that helps determine the direction more reliably */
   }else {
      iDirSet = 1; /* settle for the second node */
   }
   
   ilist = 1;
   while (!Found && ilist < Bv->N_list) {
      tPath[*N_Tri] = Bv->list[ilist];
      if (SO->FaceSetList[3*Bv->list[ilist]] == Path[iDirSet] || 
         SO->FaceSetList[3*Bv->list[ilist]+1] == Path[iDirSet] ||
         SO->FaceSetList[3*Bv->list[ilist]+2] == Path[iDirSet]) {
            Found = YUP;
      }
      ++(*N_Tri);
      ++ilist;      
   }
    
   if (!Found) {
      fprintf (SUMA_STDERR, "Error %s: Did not find next node %d in branch.\n", FuncName, Path[iDirSet]);
      SUMA_free(tPath);
      *N_Tri = 0;
      SUMA_RETURN (NULL); 
   }
  
   loc2f = *N_Tri; /* number of steps to find second node in the forward direction */

   /* do the same in the backwards direction */
   tPath[0] = Tri0;
   *N_Tri = 1;
   Found = NOPE;
   ilist = 0;
   
   if (LocalHead) fprintf(SUMA_STDERR, "%s: Going backwards looking for third node\n", FuncName);
   ilist = Bv->N_list - 1;
   while (!Found && ilist >=  0) {
      tPath[*N_Tri] = Bv->list[ilist];
      if (LocalHead) fprintf(SUMA_STDERR, "%s: trying triangle %d for node %d.\n", FuncName, Bv->list[ilist], Path[N_Path-1]);
      if (SO->FaceSetList[3*Bv->list[ilist]] == Path[iDirSet] || 
         SO->FaceSetList[3*Bv->list[ilist]+1] == Path[iDirSet] ||
         SO->FaceSetList[3*Bv->list[ilist]+2] == Path[iDirSet]) {
            Found = YUP;
      }
      ++(*N_Tri);
      --ilist;      
   }
   
   if (*N_Tri < loc2f) { 
      /* go backwards, shorter. This is based on triangle count, 
         it would be more accurate based on distance of intersected edge */
      Direction = -1;
   } else Direction = 1;
   
   /* now do the whole thing */
   
   tPath[0] = Tri0;
   *N_Tri = 1;
   Found = NOPE;
   ilist = 0;
   if (Direction == 1) { /* move forward until you reach the last node */
     if (LocalHead)   fprintf(SUMA_STDERR, "%s: Going forward, final pass \n", FuncName);
     ilist = 1;
      while (!Found && ilist < Bv->N_list) {
         tPath[*N_Tri] = Bv->list[ilist];
         if (SO->FaceSetList[3*Bv->list[ilist]] == Path[N_Path-1] || 
            SO->FaceSetList[3*Bv->list[ilist]+1] == Path[N_Path-1] ||
            SO->FaceSetList[3*Bv->list[ilist]+2] == Path[N_Path-1]) {
               Found = YUP;
         }
         ++(*N_Tri);
         ++ilist;      
      }
   } else { /* move backwards */
      if (LocalHead) fprintf(SUMA_STDERR, "%s: Going backwards, final pass \n", FuncName);
      ilist = Bv->N_list - 1;
      while (!Found && ilist >=  0) {
         tPath[*N_Tri] = Bv->list[ilist];
         if (LocalHead) fprintf(SUMA_STDERR, "%s: trying triangle %d for node %d.\n", FuncName, Bv->list[ilist], Path[N_Path-1]);
         if (SO->FaceSetList[3*Bv->list[ilist]] == Path[N_Path-1] || 
            SO->FaceSetList[3*Bv->list[ilist]+1] == Path[N_Path-1] ||
            SO->FaceSetList[3*Bv->list[ilist]+2] == Path[N_Path-1]) {
               Found = YUP;
         }
         ++(*N_Tri);
         --ilist;      
      }
      
   }   

   if (!Found) {
      fprintf (SUMA_STDERR, "Error %s: Path not completed.\n", FuncName);
      SUMA_free(tPath);
      *N_Tri = 0;
      SUMA_RETURN (NULL);
   }else {
      if (LocalHead) {
         fprintf (SUMA_STDERR,"%s: Path is %d triangles long:\n", FuncName, *N_Tri); 
         for (ilist=0; ilist< *N_Tri; ++ilist) {
            fprintf (SUMA_STDERR,"t%d\t", tPath[ilist]);
         }
         fprintf (SUMA_STDERR,"\n");
      }
   }
   SUMA_RETURN (tPath);
}   





#if 0
   /************************** BEGIN Branch Functions **************************/ 
   /* these are functions that were ported to support the first version of SUMA_Surf_Plane_Intersect was was to be identical to
   Surf_Plane_Intersect. They are left here in case I need them in the future. */
   
                  /***

                  File : SUMA_FindBranch.c
                  Author : Ziad Saad
                  Date : Thu Nov 12 16:33:34 CST 1998

                  Purpose : 
                      This is a C version of the matlab function SUMA_FindBranch2, check out the help 
                     over there.

                      This version is supposed to be faster than the working previous one called SUMA_FindBranch.c_V1
                     If you want to use SUMA_FindBranch.c_V1, you need to rename it to SUMA_FindBranch.c and make the appropriate 
                     changes in prototype.h

                     the working version of SUMA_FindBranch.c_V1 is in Backup010499 directory and should be used with 
                     Surf_Plane_Intersect.c_V1

                  Usage : 


                  Input paramters : 
                            InterMat (int **) pointer to a 2D int array of dimention [IMsz, 4]
                           IMsz (int) number or rows in InterMat
                           InterNodes (float **) pointer to a 2D float array that contains the
                              intersection nodes XYZ coordinates, size of the array is [INsz,3]
                           verbose (int) verbose flag (0/1)
                           WBsz (int *) the number of elements in WeldedBranch

                  Returns : 
                            WeldedBranch (SUMA_BRANCH *) is a pointer to structures branch that will contain 
                              the various branches. You need to pass a pointer only, allocation for this 
                              pointer should be done from the calling function.
                              see : /home/ziad/Programs/C/Z/Zlib/mystructs.h for details on
                              the fields of the structures branch

                           NOTE : The function uses static allocation for WeldedBranch
                           do not try to free WeldedBranch



                  ***/
                  SUMA_BRANCH * SUMA_FindBranch (int ** InterMat, int N_InterMat, float ** InterNodes, int ** NodeLoc_in_InterMat, int verbose,  int * WBsz)
                  {/*SUMA_FindBranch*/
                     int DBG , VeryFirstSeed, Seed, sz_Branch, kk;
                     int n_comp = 0, ntmpint, nunqrow , NodeIndex , curnode , BranchIndex;
                     int ntmpint2D_V2, N_vunq , brEnd1 , brEnd2 , i, k;
                     int tmpint2D_V2[1][2], *v, *vunq;
                     int *tmpint, *unqrow, iii, GotSeed;
                     static char FuncName[]={"SUMA_FindBranch"};
                     float Dprecision;
                     static SUMA_BRANCH * branch;
                     struct  timeval  start_time, tt_sub, start_time2;
                     float DT_WELDSUMA_BRANCH, DT_BUILDSUMA_BRANCH, DT_WELDSUMA_BRANCHONLY ,DT_FINDININTVECT, DT_VUNQ;
                     FILE *TimeOut;
                     SUMA_Boolean LocalHead = NOPE; 

                     if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

                     if (LocalHead) SUMA_disp_dmat (NodeLoc_in_InterMat, 20, 4, 1); 

                     /* open a file to output timing info and run some stats on them */
                     TimeOut = fopen("FB.TimeOut","a");   

                     DBG = 1;
                     Dprecision = 0.001;

                     VeryFirstSeed = 0;

                     /* Now you need to find the different branches */

                     Seed = VeryFirstSeed;   /* That should not matter */

                     /* Allocate for branch */
                     if (!branch)
                        {
                           branch = (SUMA_BRANCH *) SUMA_calloc(SUMA_BRANCHMAX, sizeof(SUMA_BRANCH));
                           if (!branch )
                              {
                                 fprintf (SUMA_STDERR, "Error %s: Could not allocate for branch", FuncName);
                                 SUMA_RETURN (NULL);
                              }
                        }

                     if (LocalHead) fprintf(SUMA_STDERR, "%s : Determining branches\n", FuncName);

                     /* Start timer for next function */
                     SUMA_etime(&start_time,0);

                     /* initialize the first branch */
                     BranchIndex = 0;
                     NodeIndex = 0;
                     branch[BranchIndex].start = Seed;
                     branch[BranchIndex].list[NodeIndex] = branch[BranchIndex].start;
                     curnode = branch[BranchIndex].start;
                     n_comp = N_InterMat;
                     ntmpint2D_V2 = 0;
                     while (n_comp)
                        {
                           /* see if you can find the node */
                           /*printf ("curnode = %d, n_comp = %d\n", curnode, n_comp);                   */
                           if (NodeLoc_in_InterMat[curnode][2] > -1)
                              {
                                 tmpint2D_V2[0][0] = NodeLoc_in_InterMat[curnode][2];
                                 tmpint2D_V2[0][1] = NodeLoc_in_InterMat[curnode][3];
                                 NodeLoc_in_InterMat[curnode][2] = -1;
                                 ntmpint2D_V2 = 1;
                              }
                           else
                           if (NodeLoc_in_InterMat[curnode][0] > -1)
                              {
                                 tmpint2D_V2[0][0] = NodeLoc_in_InterMat[curnode][0];
                                 tmpint2D_V2[0][1] = NodeLoc_in_InterMat[curnode][1];
                                 NodeLoc_in_InterMat[curnode][0] = -1;
                                 ntmpint2D_V2 = 1;
                              }
                           else
                              ntmpint2D_V2 = 0;

                           if (!ntmpint2D_V2)  /* Nothing found */
                              {
                                 /* store the last point as a stopping point */
                                 branch[BranchIndex].last = branch[BranchIndex].list[NodeIndex];
                                 branch[BranchIndex].listsz = NodeIndex + 1;

                                 /* start a new branch */
                                 /*pick any seed one that does not have a -1 entry in NodeLoc_in_InterMat*/
                                 iii = 0;
                                 GotSeed = 0;
                                 while (!GotSeed)
                                 {
                                    if (NodeLoc_in_InterMat[iii][2] > -1)
                                       {

                                          Seed = InterMat[NodeLoc_in_InterMat[iii][2]][NodeLoc_in_InterMat[iii][3]];
                                          GotSeed = 1;
                                       }
                                    else
                                    if (NodeLoc_in_InterMat[iii][0] > -1)
                                       {
                                          Seed = InterMat[NodeLoc_in_InterMat[iii][0]][NodeLoc_in_InterMat[iii][1]];
                                          GotSeed = 1;
                                       }
                                    else
                                       {
                                          ++iii;
                                          GotSeed = 0;
                                       }
                                 }
                                 ++BranchIndex;
                                 NodeIndex=0;
                                 branch[BranchIndex].start = Seed;
                                 branch[BranchIndex].list[NodeIndex] = branch[BranchIndex].start;
                                 curnode = branch[BranchIndex].start;
                              }
                           else /* That's a normal point, add it */
                              {
                                 ++NodeIndex;
                                 if (tmpint2D_V2[0][1]) /* take the first element */
                                    branch[BranchIndex].list[NodeIndex] = 
                                       InterMat[tmpint2D_V2[0][0]][0];
                                    else /* take second element */
                                    branch[BranchIndex].list[NodeIndex] = 
                                       InterMat[tmpint2D_V2[0][0]][1];

                                 /* make the new node current */
                                 curnode = branch[BranchIndex].list[NodeIndex];

                                 --n_comp;
                              }

                        }

                     /* now store the very last point as a stopping point */

                     branch[BranchIndex].last = branch[BranchIndex].list[NodeIndex];
                     branch[BranchIndex].listsz = NodeIndex + 1;

                     sz_Branch = BranchIndex + 1;

                     /* stop timer */
                     DT_BUILDSUMA_BRANCH = SUMA_etime(&start_time,1);

                     if (LocalHead) fprintf(SUMA_STDERR, "%s : Welding branches\n", FuncName);

                     /* now, if possible, link the branches together */
                     /* Start timer for next function */
                     SUMA_etime(&start_time,0);

                     /* initialize some variables */
                     v = (int *)SUMA_calloc(2*sz_Branch,sizeof(int));
                     if (!v)
                        {
                           fprintf (SUMA_STDERR, "Error %s: Could not allocate", FuncName);
                           SUMA_RETURN (NULL);
                        }
                     for (i=0;i<sz_Branch;++i)
                        {
                           v[i] = branch[i].start;
                           v[i+sz_Branch] = branch[i].last;
                        }


                     vunq = SUMA_UniqueInt (v, 2*sz_Branch, &N_vunq, 0);

                     for (i=0;i<N_vunq;++i)
                        {
                           /* find out how many time each end of a branch is used */

                           tmpint = SUMA_Find_inIntVect (v, 2*sz_Branch, vunq[i], &ntmpint);

                           if (ntmpint == 2)
                              {
                                 /*good, two branches can be joined together */
                                 if (tmpint[0] >= sz_Branch)
                                    {   
                                       tmpint[0] = tmpint[0] - sz_Branch;
                                       brEnd1 = 1;
                                    }
                                 else
                                    brEnd1 = 0;
                                 if (tmpint[1] >= sz_Branch)
                                    {   
                                       tmpint[1] = tmpint[1] - sz_Branch;
                                       brEnd2 = 1;
                                    }
                                 else
                                    brEnd2 = 0;

                                 if (tmpint[1] != tmpint[0])
                                    {   /*   Path is not circular, join together */

                                       SUMA_WeldBranches (branch, &sz_Branch, tmpint[0] ,tmpint[1] , brEnd1, brEnd2);

                                       for (k=0;k<sz_Branch;++k)
                                          {
                                             v[k] = branch[k].start;
                                             v[k+sz_Branch] = branch[k].last;
                                          }
                                    }
                              }
                           SUMA_free(tmpint);
                        }

                     /* Now go through and determine which branches are closed loops */
                     for (i=0;i<sz_Branch; ++i)
                        {
                           if (branch[i].start == branch[i].last)
                              branch[i].closed = 1;
                           else
                              branch[i].closed = 0;

                        }


                     *WBsz = sz_Branch; /* store the number of branches to SUMA_RETURN it */

                     /* stop timer */
                     DT_WELDSUMA_BRANCH = SUMA_etime(&start_time,1);

                     if (LocalHead) fprintf(SUMA_STDERR, "%s : Freeing allocation\n", FuncName);

                     SUMA_free(vunq);
                     SUMA_free(v);

                     /* Printout timing info on screen */
                     if (LocalHead) {
                        printf ("\n\t\t%s, time fractions :\n",FuncName);
                        printf ("\t\t\tDT_WELDSUMA_BRANCH time: %f sec\n", DT_WELDSUMA_BRANCH);
                        printf ("\t\t\t DT_BUILDSUMA_BRANCH percent time : %f sec\n",  DT_BUILDSUMA_BRANCH);
                     }
                     fprintf(TimeOut, "%f\t%f\n", 
                        DT_BUILDSUMA_BRANCH, DT_WELDSUMA_BRANCH ); 


                     fclose(TimeOut);
                     SUMA_RETURN (branch);

                  }/*SUMA_FindBranch*/

                  /***

                  File : SUMA_WeldBranches.c
                  Author : Ziad Saad
                  Date : Sat Nov 14 19:30:19 CST 1998

                  Purpose : 
                     mimics the function SUMA_WeldBranches.m, check out the help over there.

                      Except that the SUMA_RETURNeed welded branches are not in the same order as
                     those SUMA_RETURNeed by the matlab function

                  Usage : 
                   void SUMA_WeldBranches ( BRANCH *branch, int *sz_Branch, int brIndx1, int brIndx2 , int brEnd1, int brEnd2 );


                  Input paramters : 
                      branch   (BRANCH *)   a vector of structures BRANCH
                     sz_Branch   (int *)   pointer to the scalar containing the number of elements of branch
                     brIndx1   (int)   index (into branch) of the first branch to weld
                     brIndx2    (int)   index (into branch) of the second branch to weld
                     brEnd1   (int)   if 0 then weld at start of branch 1
                                    if 1 then weld at end of branch 1
                     brEnd2   (int) same as brEnd1 but for branch 2

                  Returns : 
                     nothing, but what it does is weld branch1 to branch2 and puts the welded branch in the position of
                     min(branch1, branch2). The returned branch is always one branch shorter than the branch sent into the
                     function.


                  Support : 



                  Side effects : 



                  ***/
                  void SUMA_WeldBranches ( SUMA_BRANCH *branch, int *sz_Branch, int brIndx1, int brIndx2 , int brEnd1, int brEnd2 )
                  {/*SUMA_WeldBranches*/
                     SUMA_BRANCH tmp;
                     int nlst1, nlst2, k, tmpreplace, tmpmove;
                     static char FuncName[]={"SUMA_WeldBranches"};
                     SUMA_Boolean LocalHead = NOPE;

                     if (SUMAg_CF->InOut_Notify) SUMA_DBG_IN_NOTIFY(FuncName);

                     nlst1 = branch[brIndx1].listsz;
                     nlst2 = branch[brIndx2].listsz;
                     tmp.listsz = nlst1 + nlst2 - 1;

                     if (!brEnd1  && !brEnd2)
                        {
                           tmp.last = branch[brIndx2].last;
                           tmp.start = branch[brIndx1].last;
                           for (k= nlst1; k>0; --k)
                              tmp.list[nlst1-k] =  branch[brIndx1].list[k-1];   
                           for (k=1;k<nlst2;++k) /*skip the common element */
                              tmp.list[nlst1+k-1] = branch[brIndx2].list[k];
                        }
                     else if (brEnd1  && brEnd2)
                        {
                           tmp.last = branch[brIndx2].start;
                           tmp.start = branch[brIndx1].start;
                           for (k= 0; k <nlst1; ++k)
                              tmp.list[k] = branch[brIndx1].list[k];
                           for (k=nlst2; k >1 ; --k)
                              tmp.list[nlst1+nlst2-k] = branch[brIndx2].list[k-2];
                        }   
                     else if (!brEnd1 && brEnd2)
                        {
                           tmp.last = branch[brIndx2].start;
                           tmp.start = branch[brIndx1].last;
                           for (k=nlst1; k > 0; --k)
                              tmp.list[nlst1 - k] = branch[brIndx1].list[k-1];
                           for (k=nlst2; k > 1; --k)
                              tmp.list[nlst1+nlst2-k] = branch[brIndx2].list[k-2];
                        }
                     else if (brEnd1 && !brEnd2)
                        {
                           tmp.last = branch[brIndx2].last;
                           tmp.start = branch[brIndx1].start;
                           for (k=0;k<nlst1;++k)
                              tmp.list[k] = branch[brIndx1].list[k];
                           for (k=0;k<nlst2-1;++k)
                              tmp.list[nlst1+k] = branch[brIndx2].list[k+1];
                        }

                     /* decide where to put the welded branch and whether to move the last branch (or the one before it) up */
                     if (brIndx1 > brIndx2)
                        {
                           tmpreplace = brIndx2;
                           tmpmove = brIndx1;
                        }
                     else
                        {
                           tmpreplace = brIndx1;
                           tmpmove = brIndx2;
                        }
                     /* replace branch[tmpreplace]   with tmp */
                     branch[tmpreplace].start = tmp.start;
                     branch[tmpreplace].last = tmp.last;
                     branch[tmpreplace].listsz = tmp.listsz;
                     for(k=0;k<branch[tmpreplace].listsz;++k)
                        branch[tmpreplace].list[k] = tmp.list[k];

                     /*copy branch[sz_Branch-1] (the last branch) into position brIndx2 */
                     /*by now, tmpmove is definetly larger than tmpreplace*/
                     /* if tmpmove is not the last branch, then move the last branch up one*/
                     /* otherwise, no need to move anything */

                     if (tmpmove < *sz_Branch-1)
                        {
                           branch[tmpmove].start = branch[*sz_Branch-1].start;
                           branch[tmpmove].last = branch[*sz_Branch-1].last;
                           branch[tmpmove].listsz = branch[*sz_Branch-1].listsz;
                           for(k=0;k<branch[tmpmove].listsz;++k)
                              branch[tmpmove].list[k] = branch[*sz_Branch-1].list[k];
                        }

                     /* change the size of the branch vector */
                     --*sz_Branch;

                     SUMA_RETURNe;

                  }/*SUMA_WeldBranches*/

   /************************** END Branch Functions **************************/                   
#endif 
