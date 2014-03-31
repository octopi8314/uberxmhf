/*
 * @XMHF_LICENSE_HEADER_START@
 *
 * eXtensible, Modular Hypervisor Framework (XMHF)
 * Copyright (c) 2009-2012 Carnegie Mellon University
 * Copyright (c) 2010-2012 VDG Inc.
 * All Rights Reserved.
 *
 * Developed by: XMHF Team
 *               Carnegie Mellon University / CyLab
 *               VDG Inc.
 *               http://xmhf.org
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in
 * the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the names of Carnegie Mellon or VDG Inc, nor the names of
 * its contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @XMHF_LICENSE_HEADER_END@
 */

// XMHF "rich" (SMP) guest component implementation
// takes care of initializing and booting up the "rich" guest
// author: amit vasudevan (amitvasudevan@acm.org)

#include <xmhf.h> 


static void xmhf_smpguest_initialize_helper(context_desc_t context_desc){
		//initialize CPU
		xmhf_baseplatform_cpuinitialize();

		//initialize partition monitor (i.e., hypervisor) for this CPU
		//xmhf_partition_initializemonitor(vcpu);
		xmhf_partition_initializemonitor(context_desc);

		//setup guest OS state for partition
		//xmhf_partition_setupguestOSstate(vcpu);
		xmhf_partition_setupguestOSstate(context_desc);

		//initialize memory protection for this core
		xmhf_memprot_initialize(context_desc);		
}


//initialize environment to boot "rich" guest
void xmhf_smpguest_initialize(context_desc_t context_desc){
	static u32 lock_aps_in_partition = 1;
	static u32 aps_in_partition=0;
	static u32 lock_initaps_for_rich_guest = 1;
	static bool initaps_for_rich_guest=false;

  //BSP
  if(context_desc.cpu_desc.isbsp){
		//setup CPU for rich-guest
		xmhf_smpguest_initialize_helper(context_desc);

		//ok now that we are done initializing on BSP, let APs start their
		//initialization and get into the partition
		spin_lock(&lock_initaps_for_rich_guest);
		initaps_for_rich_guest=true;
		spin_unlock(&lock_initaps_for_rich_guest);

		//wait for APs to finish initialization just before getting
		//into the partition
		while(aps_in_partition < (g_midtable_numentries-1));
  
  }else{
		//we are an AP, wait for BSP to signal that it is safe for us to proceed
		while(!initaps_for_rich_guest);
		
		//setup CPU for rich-guest
		xmhf_smpguest_initialize_helper(context_desc);
	  
		//we are an AP, so simply increment the AP counter and enter the partition 
		spin_lock(&lock_aps_in_partition);
		aps_in_partition++;
		spin_unlock(&lock_aps_in_partition);
  }	

  //start partition (guest)
  printf("\n%s[%02x]: starting partition...", __FUNCTION__, context_desc.cpu_desc.id);
  xmhf_partition_start(context_desc);
}


//quiesce interface to switch all guest cores into hypervisor mode
static void xmhf_smpguest_quiesce(VCPU *vcpu) __attribute__((unused));
static void xmhf_smpguest_quiesce(VCPU *vcpu){
	xmhf_smpguest_arch_quiesce(vcpu);
}

//endquiesce interface to resume all guest cores after a quiesce
static void xmhf_smpguest_endquiesce(VCPU *vcpu) __attribute__((unused));
static void xmhf_smpguest_endquiesce(VCPU *vcpu){
	xmhf_smpguest_arch_endquiesce(vcpu);
}


//walk guest page tables; returns pointer to corresponding guest physical address
//note: returns 0xFFFFFFFF if there is no mapping
u8 * xmhf_smpguest_walk_pagetables(context_desc_t context_desc, u32 vaddr){
	u8 *retvalue;
	retvalue = xmhf_smpguest_arch_walk_pagetables(context_desc, vaddr);
	return retvalue;
}
