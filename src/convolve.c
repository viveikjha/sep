/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
* This file is part of SEP
*
* Copyright 1993-2011 Emmanuel Bertin -- IAP/CNRS/UPMC
* Copyright 2014 SEP developers
*
* SEP is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* SEP is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with SEP.  If not, see <http://www.gnu.org/licenses/>.
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "sep.h"
#include "sepcore.h"
#include "extract.h"

/* Convolve one line of an image with a given kernel.
 *
 * buf : arraybuffer struct containing buffer of data to convolve, and image
         dimension metadata.
 * conv : convolution kernel
 * convw, convh : width and height of conv
 * buf : output convolved line (buf->dw elements long)
 */
int convolve(arraybuffer *buf, int y, float *conv, int convw, int convh,
             PIXTYPE *out)
{
  int convw2, convn, cx, cy, i, dcx, y0;
  PIXTYPE *line;    /* current line in input buffer */
  PIXTYPE *outend;  /* end of output buffer */
  PIXTYPE *src, *dst, *dstend;

  outend = out + buf->dw;
  convw2 = convw/2;
  y0 = y - convh/2;  /* start line in image */

  /* Cut off top of kernel if it extends beyond image */
  if (y0 + convh > buf->dh)
    convh = buf->dh - y0;

  /* cut off bottom of kernel if it extends beyond image */
  if (y0 < 0)
    {
      convh = convh + y0;
      conv += convw*(-y0);
      y0 = 0;
    }

  /* check that buffer has needed lines */
  if ((y0 < buf->yoff) || (y0+convh > buf->yoff + buf->bh))
    return LINE_NOT_IN_BUF;

  memset(out, 0, buf->dw*sizeof(PIXTYPE));  /* initialize output to zero */

  /* loop over pixels in the convolution kernel */
  convn = convw * convh;
  for (i=0; i<convn; i++)
    {
      cx = i % convw;  /* x index in conv kernel */
      cy = i / convw;  /* y index in conv kernel */
      line = buf->bptr + buf->bw * (y0 - buf->yoff + cy); /* start of line */

      /* get start and end positions in the source and target line */
      dcx = cx - convw2; /* offset of conv pixel from conv center;
                            determines offset between in and out line */
      if (dcx >= 0)
	{
	  src = line + dcx;
	  dst = out;
	  dstend = outend - dcx;
	}
      else
	{
	  src = line;
	  dst = out - dcx;
	  dstend = outend;
	}

      /* multiply and add the values */
      while (dst < dstend)
	*(dst++) += conv[i] * *(src++);
    }

  return RETURN_OK;
}


/* Apply a matched filter to one line of an image with a given kernel.
 *
 * imbuf : arraybuffer for data array
 * nbuf : arraybuffer for noise array
 * y : line to apply the matched filter to in an image
 * conv : convolution kernel
 * convw, convh : width and height of conv
 * outnum : output numerator line (`imbuf->dw` elements long)
 * outdenom : output denominator line (`imbuf->dw` elements long)
 *
 * imbuf and nbuf should have same data dimensions and be on the same line
 * (their `yoff` fields should be the same).
 */
int matched_filter(arraybuffer *imbuf, arraybuffer *nbuf, int y,
                   float *conv, int convw, int convh,
                   PIXTYPE *outnum, PIXTYPE *outdenom)
{
  int convw2, convn, cx, cy, i, dcx, y0;
  PIXTYPE imval, nval;
  PIXTYPE *imline, *nline;    /* current line in input buffer */
  PIXTYPE *outnumend, *outdenomend;  /* end of output buffer */
  PIXTYPE *src_im, *src_n, *dst_num, *dst_denom, *dst_num_end;

  outnumend = outnum + imbuf->dw;
  outdenomend = outdenom + imbuf->dw;
  convw2 = convw/2;
  y0 = y - convh/2;  /* start line in image */

  /* Cut off top of kernel if it extends beyond image */
  if (y0 + convh > imbuf->dh)
    convh = imbuf->dh - y0;

  /* cut off bottom of kernel if it extends beyond image */
  if (y0 < 0)
    {
      convh = convh + y0;
      conv += convw*(-y0);
      y0 = 0;
    }

  /* check that buffer has needed lines */
  if ((y0 < imbuf->yoff) || (y0+convh > imbuf->yoff + imbuf->bh) ||
      (y0 < nbuf->yoff)  || (y0+convh > nbuf->yoff + nbuf->bh))
    return LINE_NOT_IN_BUF;

  /* check that image and noise buffer match */
  if ((imbuf->yoff != nbuf->yoff) || (imbuf->dw != nbuf->dw))
    return LINE_NOT_IN_BUF;  /* TODO new error status code */

  /* initialize output buffers to zero */
  memset(outnum, 0, imbuf->dw*sizeof(PIXTYPE));
  memset(outdenom, 0, imbuf->dw*sizeof(PIXTYPE));

  /* loop over pixels in the convolution kernel */
  convn = convw * convh;
  for (i=0; i<convn; i++)
    {
      cx = i % convw;  /* x index in conv kernel */
      cy = i / convw;  /* y index in conv kernel */
      imline = imbuf->bptr + imbuf->bw * (y0 - imbuf->yoff + cy);
      nline = nbuf->bptr + nbuf->bw * (y0 - nbuf->yoff + cy);

      /* get start and end positions in the source and target line */
      dcx = cx - convw2; /* offset of conv pixel from conv center;
                            determines offset between in and out line */
      if (dcx >= 0)
	{
	  src_im = imline + dcx;
          src_n = nline + dcx;
	  dst_num = outnum;
          dst_denom = outdenom;
	  dst_num_end = outnumend - dcx;
	}
      else
	{
	  src_im = imline;
          src_n = nline;
	  dst_num = outnum - dcx;
          dst_denom = outdenom - dcx;
	  dst_num_end = outnumend;
	}

      /* actually calculate values */
      while (dst_num < dst_num_end)
        {
          imval = *src_im;
          nval = *src_n;
          if (nval != 0.0)
            {
              *dst_num   += conv[i] * imval / (nval * nval);
              *dst_denom += conv[i] * conv[i] / (nval * nval);
            }
          src_im++;
          src_n++;
          dst_num++;
          dst_denom++;
        }
    }  /* close loop over convolution kernel */

  /* take the square root of the denominator buffer */
  for (dst_denom = outdenom; dst_denom < outdenomend; dst_denom++)
      *dst_denom = sqrt(*dst_denom);

  return RETURN_OK;
}
