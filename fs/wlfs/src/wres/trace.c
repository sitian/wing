/*      trace.c
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

#include "trace.h"

char op_unknown[32] = "UNKNOWN";
char op_list[WRES_NR_OPERATIONS][32] = {
		"UNUSED0",
		"MSGSND",
		"MSGRCV",
		"MSGCTL",
		"SEMOP",
		"SEMCTL",
		"SEMEXIT",
		"SHMFAULT",
		"SHMCTL",
		"UNUSED1",
		"TSKGET",
		"MSGGET",
		"SEMGET",
		"SHMGET",
		"TSKPUT",
		"RELEASE",
		"PROBE",
		"UNUSED2",
		"SIGNIN",
		"SYNCTIME",
		"REPLY",
		"CANCEL",
		"UNUSED3"};

char shmfault_op_list[WRES_SHM_NR_OPERATIONS][32] = {
		"UNUSED0",
		"PROPOSE",
		"CHK_OWNER",
		"CHK_HOLDER",
		"NOTIFY_OWNER",
		"NOTIFY_HOLDER",
		"NOTIFY_PROPOSER"};

char *wres_op2str(wres_op_t op)
{
	if (op < WRES_NR_OPERATIONS)
		return op_list[op];
	else
		return op_unknown;
}

char *wres_shmfault2str(wres_op_t op)
{
	if (op < WRES_SHM_NR_OPERATIONS)
		return shmfault_op_list[op];
	else
		return op_unknown;
}


void wres_trace(wres_req_t *req)
{
	static int count = 0;
	static int shmfault_ops[WRES_SHM_NR_OPERATIONS] = {0};
	wres_op_t op = wres_get_op(&req->resource);
	
	if (WRES_OP_SHMFAULT == op) {
		wres_shmfault_args_t *args = (wres_shmfault_args_t *)req->buf;
		if (args->cmd >= WRES_SHM_NR_OPERATIONS)
			return;
		shmfault_ops[args->cmd]++;
	}

	count++;
	if (WRES_TRACE_INTERVAL == count) {
		int i;
		
		count = 0;
		printf("%s:", wres_op2str(WRES_OP_SHMFAULT));
		for (i = 0; i < WRES_SHM_NR_OPERATIONS - 1; i++)
			printf("%s=%d, ", wres_shmfault2str(i), shmfault_ops[i]);
		printf("%s=%d\n", wres_shmfault2str(i), shmfault_ops[i]);
	}
}
