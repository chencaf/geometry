/* Copyright (C) 2000 Kai Habel
** Copyright R-version (C) 2005 Raoul Grasman 
**                     (C) 2013-2015, 2017, 2018 David Sterratt
**                     (C) 2018 Pavlo Mozharovskyi
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
*/

/*
29. July 2000 - Kai Habel: first release
2002-04-22 Paul Kienzle
* Use warning(...) function rather than writing to cerr

23. May 2005 - Raoul Grasman: ported to R
* Changed the interface for R

02. February 2018 - Pavlo Mozharovskyi: added non-triangulated output
*/

#include "Rgeometry.h"

SEXP C_convhulln(const SEXP p, const SEXP options, const SEXP returnNonTriangulatedFacets, const SEXP tmpdir)
{
  /* Initialise return values */
  
  SEXP retval, area, vol, normals, retlist, retnames;
  retval = area = vol = normals = retlist = R_NilValue;
  int retlen = 1; /* Indicies are output by default. If other outputs are
                     selected this value is incremented */

  /* Run Qhull */
  
  qhT *qh= (qhT*)malloc(sizeof(qhT));
  char errstr1[100], errstr2[100];
  unsigned int dim, n;
  char cmd[50] = "qhull";
  int exitcode = qhullNewQhull(qh, p, cmd,  options, tmpdir, &dim, &n, errstr1, errstr2);

  /* Extract information from output */
  
  int i, j, *idx;
  if (!exitcode) {  /* 0 if no error from qhull */

    facetT *facet;              /* set by FORALLfacets */
    vertexT *vertex, **vertexp; /* set by FORALLfacets */
    unsigned int n = qh->num_facets;

    unsigned int nVertexMax = 0;

    /* If parameter (flag) returnNonTriangulatedFacets is set, count the
       number of columns in the output matrix of vertices as the maximal
       number of vertices in a facet, then allocate the matrix. */
    if (INTEGER(returnNonTriangulatedFacets)[0] > 0){
      i=0;
      FORALLfacets {
        j=0;
        FOREACHvertex_ (facet->vertices) {
          j++;
        }
        if (j > nVertexMax){
          nVertexMax = j;
        }
      }
    }else{
      /* If parameter (flag) returnNonTriangulatedFacets is not set, the
         number of columns equals dimension. */
      nVertexMax = dim;
    }
    retval = PROTECT(allocMatrix(INTSXP, n, nVertexMax));
    idx = (int *) R_alloc(n*nVertexMax,sizeof(int));

    if (hasPrintOption(qh, qh_PRINTnormals)) {
      normals = PROTECT(allocMatrix(REALSXP, n, dim+1));
      retlen++;
    }
    
    qh_vertexneighbors(qh);

    i=0; /* Facet counter */
    FORALLfacets {
      j=0;
      /* qh_printfacet(stdout,facet); */
      FOREACHvertex_ (facet->vertices) {
        /* qh_printvertex(stdout,vertex); */
        if (INTEGER(returnNonTriangulatedFacets)[0] == 0 && j >= dim)
          warning("extra vertex %d of facet %d = %d",
                  j++,i,1+qh_pointid(qh, vertex->point));
        else
          idx[i+n*j++] = 1 + qh_pointid(qh, vertex->point);
      }
      if (j < dim) warning("facet %d only has %d vertices",i,j);
      while (j < nVertexMax){
        idx[i+n*j++] = 0; /* Fill with zeros for the moment */
      }
      
      /* Output normals */
      if (hasPrintOption(qh, qh_PRINTnormals)) {
        if (facet->normal) {
          for (j=0; j<dim; j++) {
            REAL(normals)[i+nrows(normals)*j] = facet->normal[j];
          }
          REAL(normals)[i+nrows(normals)*dim] = facet->offset;
        } else {
          for (j=0; j<=dim; j++) {
            REAL(normals)[i+nrows(normals)*j] = 0;
          }
        }
      }

      i++; /* Increment facet counter */
    }
    j=0;
    for(i=0;i<nrows(retval);i++)
      for(j=0;j<ncols(retval);j++)
        if (idx[i+n*j] > 0){
          INTEGER(retval)[i+nrows(retval)*j] = idx[i+n*j];
        }else{
          INTEGER(retval)[i+nrows(retval)*j] = NA_INTEGER;
        }

    /* Return area and volume */
    if (qh->totarea != 0.0) {
      area = PROTECT(allocVector(REALSXP, 1));
      REAL(area)[0] = qh->totarea;
      retlen++;
    }
    if (qh->totvol != 0.0) {
      vol = PROTECT(allocVector(REALSXP, 1));
      REAL(vol)[0] = qh->totvol;
      retlen++;
    }

    /* Make a list if there is area or volume */
    i = 0;                      /* Output counter */
    if (retlen > 1) {
      retlist = PROTECT(allocVector(VECSXP, retlen));
      retnames = PROTECT(allocVector(VECSXP, retlen));
      retlen += 2;
      SET_VECTOR_ELT(retlist, i, retval);
      SET_VECTOR_ELT(retnames, i, mkChar("hull"));
      if (qh->totarea != 0.0) {
        i++;
        SET_VECTOR_ELT(retlist, i, area);
        SET_VECTOR_ELT(retnames, i, mkChar("area"));
      }
      if (qh->totvol != 0.0) {
        i++;
        SET_VECTOR_ELT(retlist, i, vol);
        SET_VECTOR_ELT(retnames, i, mkChar("vol"));
      }
      if (hasPrintOption(qh, qh_PRINTnormals)) {
        i++;
        SET_VECTOR_ELT(retlist, i, normals);
        SET_VECTOR_ELT(retnames, i, mkChar("normals"));
      }
      setAttrib(retlist, R_NamesSymbol, retnames);
    } else {
      retlist = retval;
    }

  }

  /* Register qhullFinalizer() for garbage collection and attach a
     pointer to the hull as an attribute for future use. */
  SEXP ptr, tag;
  tag = PROTECT(allocVector(STRSXP, 1));
  SET_STRING_ELT(tag, 0, mkChar("convhull"));
  ptr = PROTECT(R_MakeExternalPtr(qh, tag, R_NilValue));
  if (exitcode) {
    qhullFinalizer(ptr);
  } else {
    R_RegisterCFinalizerEx(ptr, qhullFinalizer, TRUE);
    setAttrib(retlist, tag, ptr);
  }
  UNPROTECT(retlen + 2);

  if (exitcode) {
    error("Received error code %d from qhull. Qhull error:\n    %s    %s", exitcode, errstr1, errstr2);
  }
  return retlist;
}
