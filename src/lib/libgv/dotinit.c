#include "graphvizconfig.h"
/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/*************************************************************************
 * Copyright (c) 2011 AT&T Intellectual Property 
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors: See CVS logs. Details at http://www.graphviz.org/
 *************************************************************************/


#include <time.h>
#include "dot.h"
#include "aspect.h"

#ifdef WITH_CGRAPH
static void
dot_init_subg(graph_t * g)
{
    graph_t* subg;

    if ((g != agroot(g)))
	agbindrec(g, "Agraphinfo_t", sizeof(Agraphinfo_t), TRUE);
    for (subg = agfstsubg(g); subg; subg = agnxtsubg(subg)) {
	dot_init_subg(subg);
    }
}
#endif


static void 
dot_init_node(node_t * n)
{
#ifdef WITH_CGRAPH
    agbindrec(n, "Agnodeinfo_t", sizeof(Agnodeinfo_t), TRUE);	//graph custom data
#endif /* WITH_CGRAPH */
    common_init_node(n);
    gv_nodesize(n, GD_flip(agraphof(n)));
    alloc_elist(4, ND_in(n));
    alloc_elist(4, ND_out(n));
    alloc_elist(2, ND_flat_in(n));
    alloc_elist(2, ND_flat_out(n));
    alloc_elist(2, ND_other(n));
    ND_UF_size(n) = 1;
}

static void 
dot_init_edge(edge_t * e)
{
    char *tailgroup, *headgroup;
#ifdef WITH_CGRAPH
    agbindrec(e, "Agedgeinfo_t", sizeof(Agedgeinfo_t), TRUE);	//graph custom data
#endif /* WITH_CGRAPH */
    common_init_edge(e);

    ED_weight(e) = late_double(e, E_weight, 1.0, 0.0);
    tailgroup = late_string(agtail(e), N_group, "");
    headgroup = late_string(aghead(e), N_group, "");
    ED_count(e) = ED_xpenalty(e) = 1;
    if (tailgroup[0] && (tailgroup == headgroup)) {
	ED_xpenalty(e) = CL_CROSS;
	ED_weight(e) *= 100;
    }
    if (nonconstraint_edge(e)) {
	ED_xpenalty(e) = 0;
	ED_weight(e) = 0;
    }

    ED_showboxes(e) = late_int(e, E_showboxes, 0, 0);
    ED_minlen(e) = late_int(e, E_minlen, 1, 0);
}

void 
dot_init_node_edge(graph_t * g)
{
    node_t *n;
    edge_t *e;

    for (n = agfstnode(g); n; n = agnxtnode(g, n))
	dot_init_node(n);
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
	for (e = agfstout(g, n); e; e = agnxtout(g, e))
	    dot_init_edge(e);
    }
}

#if 0				/* not used */
static void free_edge_list(elist L)
{
    edge_t *e;
    int i;

    for (i = 0; i < L.size; i++) {
	e = L.list[i];
	free(e);
    }
}
#endif

static void 
dot_cleanup_node(node_t * n)
{
    free_list(ND_in(n));
    free_list(ND_out(n));
    free_list(ND_flat_out(n));
    free_list(ND_flat_in(n));
    free_list(ND_other(n));
    free_label(ND_label(n));
    if (ND_shape(n))
	ND_shape(n)->fns->freefn(n);
#ifndef WITH_CGRAPH
    memset(&(n->u), 0, sizeof(Agnodeinfo_t));
#else /* WITH_CGRAPH */
    agdelrec(n, "Agnodeinfo_t");	
#endif /* WITH_CGRAPH */
}

static void free_virtual_edge_list(node_t * n)
{
    edge_t *e;
    int i;

    for (i = ND_in(n).size - 1; i >= 0; i--) {
	e = ND_in(n).list[i];
	delete_fast_edge(e);
#ifdef WITH_CGRAPH
	free(e->base.data);
#endif
	free(e);
    }
    for (i = ND_out(n).size - 1; i >= 0; i--) {
	e = ND_out(n).list[i];
	delete_fast_edge(e);
#ifdef WITH_CGRAPH
	free(e->base.data);
#endif
	free(e);
    }
}

static void free_virtual_node_list(node_t * vn)
{
    node_t *next_vn;

    while (vn) {
	next_vn = ND_next(vn);
	free_virtual_edge_list(vn);
	if (ND_node_type(vn) == VIRTUAL) {
	    free_list(ND_out(vn));
	    free_list(ND_in(vn));
#ifdef WITH_CGRAPH
	    free(vn->base.data);
#endif
	    free(vn);
	}
	vn = next_vn;
    }
}

static void 
dot_cleanup_graph(graph_t * g)
{
    int i;
#ifndef WITH_CGRAPH
    graph_t *clust;
    int c;
    for (c = 1; c <= GD_n_cluster(g); c++) {
	clust = GD_clust(g)[c];
	GD_parent(clust) = NULL;
	dot_cleanup(clust);
    }
#else
    graph_t *subg;
    for (subg = agfstsubg(g); subg; subg = agnxtsubg(subg)) {
	dot_cleanup_graph(subg);
    }
#endif
    if (GD_clust(g)) free (GD_clust(g));
    if (GD_rankleader(g)) free (GD_rankleader(g));

    free_list(GD_comp(g));
    if (GD_rank(g)) {
	for (i = GD_minrank(g); i <= GD_maxrank(g); i++)
	    free(GD_rank(g)[i].av);
	if (GD_minrank(g) == -1)
	    free(GD_rank(g)-1);
	else
	    free(GD_rank(g));
    }
    if (g != agroot(g)) 
#ifndef WITH_CGRAPH
	memset(&(g->u), 0, sizeof(Agraphinfo_t));
#else /* WITH_CGRAPH */
	agdelrec(g,"Agraphinfo_t");
#endif /* WITH_CGRAPH */
}

/* delete the layout (but retain the underlying graph) */
void dot_cleanup(graph_t * g)
{
    node_t *n;
    edge_t *e;

    free_virtual_node_list(GD_nlist(g));
    for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
	for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
	    gv_cleanup_edge(e);
	}
	dot_cleanup_node(n);
    }
    dot_cleanup_graph(g);
}

#ifdef DEBUG
int
fastn (graph_t * g)
{
    node_t* u;
    int cnt = 0;
    for (u = GD_nlist(g); u; u = ND_next(u)) cnt++;
    return cnt;
}

static void
dumpRanks (graph_t * g)
{
    int i, j;
    node_t* u;
    rank_t *rank = GD_rank(g);
    int rcnt = 0;
    for (i = GD_minrank(g); i <= GD_maxrank(g); i++) {
	fprintf (stderr, "[%d] :", i);
	for (j = 0; j < rank[i].n; j++) {
	    u = rank[i].v[j];
            rcnt++;
	    if (streq(u->name,"virtual"))
	        fprintf (stderr, " %x", u);
	    else
	        fprintf (stderr, " %s", u->name);
      
        }
	fprintf (stderr, "\n");
    }
    fprintf (stderr, "count %d rank count = %d\n", fastn(g), rcnt);
}
#endif

#define MIN_AR 1.0
#define MAX_AR 20.0
#define DEF_PASSES 5

static aspect_t*
setAspect (Agraph_t * g, aspect_t* adata)
{
    double rv;
    char *p;
    int r, passes = DEF_PASSES;

    p = agget (g, "aspect");

    if (!p || ((r = sscanf (p, "%lf,%d", &rv, &passes)) <= 0)) {
	adata->nextIter = 0;
	adata->badGraph = 0;
	return NULL;
    }
    
    if (rv < MIN_AR) rv = MIN_AR;
    else if (rv > MAX_AR) rv = MAX_AR;
    adata->targetAR = rv;
    adata->nextIter = -1;
    adata->nPasses = passes;
    adata->badGraph = 0;

    if (Verbose) 
        fprintf(stderr, "Target AR = %g\n", adata->targetAR);

    return adata;
}

#ifdef WITH_CGRAPH
static void
remove_from_rank (Agraph_t * g, Agnode_t* n)
{
    Agnode_t* v = NULL;
    int j, rk = ND_rank(n);

    for (j = 0; j < GD_rank(g)[rk].n; j++) {
	v = GD_rank(g)[rk].v[j];
	if (v == n) {
	    for (j++; j < GD_rank(g)[rk].n; j++) {
		GD_rank(g)[rk].v[j-1] = GD_rank(g)[rk].v[j];
	    }
	    GD_rank(g)[rk].n--;
	    break;
	}
    }
    assert (v == n);  /* if found */
}

/* removeFill:
 * This removes all of the fill nodes added in mincross.
 * It appears to be sufficient to remove them only from the
 * rank array and fast node list of the root graph.
 */
static void
removeFill (Agraph_t * g)
{
    Agnode_t* n;
    Agnode_t* nxt;
    Agraph_t* sg = agsubg (g, "_new_rank", 0);

    if (!sg) return;
    for (n = agfstnode(sg); n; n = nxt) {
	nxt = agnxtnode(sg, n);
	delete_fast_node (g, n);
	remove_from_rank (g, n);
	dot_cleanup_node (n);
	agdelnode(g, n);
    }
    agdelsubg (g, sg);

}
#endif

void dot_layout(Agraph_t * g)
{
    aspect_t aspect;
    aspect_t* asp;

    setEdgeType (g, ET_SPLINE);
    asp = setAspect (g, &aspect);

#ifdef WITH_CGRAPH
    dot_init_subg(g);
#endif

    dot_init_node_edge(g);

    do {
        dot_rank(g, asp);
	if (aspect.badGraph) {
	    agerr(AGWARN, "dot does not support the aspect attribute for disconnected graphs or graphs with clusters\n");
	    asp = NULL;
	    aspect.nextIter = 0;
	}
        dot_mincross(g, (asp != NULL));
        dot_position(g, asp);
	aspect.nPasses--;
    } while (aspect.nextIter && aspect.nPasses);
#ifdef WITH_CGRAPH
    if (GD_flags(g) & NEW_RANK)
	removeFill (g);
#endif
    dot_sameports(g);
    dot_splines(g);
    if (mapbool(agget(g, "compound")))
	dot_compoundEdges(g);
    dotneato_postprocess(g);

}
