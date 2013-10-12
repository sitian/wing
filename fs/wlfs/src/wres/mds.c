/*      mds.c
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

#include "mds.h"

int mds_stat = 0;
int mds_port = 2181; 
char mds_addr[16] = {0};


int wres_mds_do_resolve()
{
	return 0;
}


int wres_mds_resolve()
{
#ifdef ZOOKEEPER
	return wres_zk_resolve(mds_addr, mds_port);
#else
	return wres_mds_do_resolve();
#endif
}


int wres_mds_check()
{
	if (!wres_mds_resolve())
		mds_stat = WRES_STAT_INIT;
	else {
		mds_stat = 0;
		return -EINVAL;
	}
	return 0;
}


int wres_mds_do_read(char *addr, char *path, char *buf, int len)
{
	return 0;
}


int wres_mds_read(char *path, char *buf, int len)
{
	int ret;

	if ((mds_stat != WRES_STAT_INIT) && wres_mds_check())
		return -EINVAL;
	
#ifdef ZOOKEEPER
	ret = wres_zk_read(mds_addr, path, buf, len);
#else
	ret = wres_mds_do_read(mds_addr, path, buf, len);
#endif
	if (ret)
		wres_mds_check();
	return ret;
}


int wres_mds_do_write(char *addr, char *path, char *buf, int len)
{
	return 0;
}


int wres_mds_write(char *path, char *buf, int len)
{
	int ret;
	
	if ((mds_stat != WRES_STAT_INIT) && wres_mds_check())
		return -EINVAL;
	
#ifdef ZOOKEEPER
	ret = wres_zk_write(mds_addr, path, buf, len);
#else
	ret = wres_mds_do_write(mds_addr, path, buf, len);
#endif
	if (ret)
		wres_mds_check();
	return ret;
}


int wres_mds_do_access(char *addr, char *path)
{
	return 0;
}


int wres_mds_access(char *path)
{
	int ret;
	
	if ((mds_stat != WRES_STAT_INIT) && wres_mds_check())
		return -EINVAL;
	
#ifdef ZOOKEEPER
	ret = wres_zk_access(mds_addr, path);
#else
	ret = wres_mds_do_access(mds_addr, path);
#endif
	if (ret)
		wres_mds_check();
	return ret;
}


int wres_mds_do_delete(char *addr, char *path)
{
	return 0;
}


int wres_mds_delete(char *path)
{
	int ret;
	
	if ((mds_stat != WRES_STAT_INIT) && wres_mds_check())
		return -EINVAL;

#ifdef ZOOKEEPER
	ret = wres_zk_delete(mds_addr, path);
#else
	ret = wres_mds_do_delete(mds_addr, path);
#endif
	if (ret)
		wres_mds_check();
	return ret;
}


int wres_mds_do_count(char *addr, char *path)
{
	return 0;
}


int wres_mds_count(char *path)
{
	int ret;
	
	if ((mds_stat != WRES_STAT_INIT) && wres_mds_check())
		return -EINVAL;

#ifdef ZOOKEEPER
	ret = wres_zk_count(mds_addr, path);
#else
	ret = wres_mds_do_count(mds_addr, path);
#endif
	if (ret)
		wres_mds_check();
	return ret;
}


int wres_mds_do_get_max_key(char *addr, char *path)
{
	return 0;
}


int wres_mds_get_max_key(char *path)
{
	int ret;
	
	if ((mds_stat != WRES_STAT_INIT) && wres_mds_check())
		return -EINVAL;

#ifdef ZOOKEEPER
	ret = wres_zk_get_max_key(mds_addr, path);
#else
	ret = wres_mds_do_get_max_key(mds_addr, path);
#endif
	if (ret)
		wres_mds_check();
	return ret;
}

