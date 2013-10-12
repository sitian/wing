/*      line.c
 *      
 *      Copyright (C) 2013 Yi-Wei Ci <ciyiwei@hotmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#include "line.h"

wres_digest_t wres_line_get_digest(char *buf)
{
	int i, pos = 0;
	wres_digest_t digest = 0;
	size_t buflen = WRES_LINE_SIZE;
	
	if (buflen < sizeof(wres_digest_t) * WRES_LINE_NR_SAMPLES) {
		while (pos < buflen) {
			if (pos + sizeof(wres_digest_t) < buflen)
				digest ^= *(wres_digest_t *)&buf[pos];
			else {
				wres_digest_t val;
				
				memcpy(&val, &buf[pos], buflen - pos);
				digest ^= val;
			}
			pos += sizeof(wres_digest_t);
		}
	} else {
		int list[WRES_LINE_NR_SAMPLES] = {0};
		
		for (i = 0; i < WRES_LINE_NR_SAMPLES; i++) {
			int start = pos;
			
			do {
				wres_digest_t val;
				
				if (pos + sizeof(wres_digest_t) < buflen)
					memcpy(&val, &buf[pos], sizeof(wres_digest_t));
				else
					memcpy(&val, &buf[pos], buflen - pos);
				
				if (val) {
					int j;
					
					for (j = 0; j < i; j++)
						if (list[j] == val)
							break;
					if (j == i) {
						digest ^= val;
						list[i] = val;
						pos = (pos + val) % buflen;
						break;
					}
				}
				pos++;
				if (pos == buflen)
					pos = 0;
			} while (pos != start);
			
			if (pos == start)
				break;
		}
	}
	return digest;
}
