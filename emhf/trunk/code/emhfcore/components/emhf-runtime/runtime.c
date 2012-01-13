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
 * This file is part of the EMHF historical reference
 * codebase, and is released under the terms of the
 * GNU General Public License (GPL) version 2.
 * Please see the LICENSE file for details.
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

// runtime.c
// author: amit vasudevan (amitvasudevan@acm.org)

//---includes-------------------------------------------------------------------
#include <emhf.h> 

//---runtime main---------------------------------------------------------------
void cstartup(void){
	u32 cpu_vendor;

	//get CPU vendor
	cpu_vendor = emhf_baseplatform_getcpuvendor();

	//initialize global runtime variables including Runtime Parameter Block (rpb)
	runtime_globals_init();

	//initialize isolation layer and EMHF library interface abstraction
	if(cpu_vendor == CPU_VENDOR_INTEL){
		//g_isl = &g_isolation_layer_vmx;
		g_libemhf = &g_emhf_library_vmx;
	}else{
		//g_isl = &g_isolation_layer_svm; 
		g_libemhf = &g_emhf_library_svm;
	}

	//setup debugging	
/*#ifdef __DEBUG_SERIAL__
        // need to reinitialize serial port on some systems.
           (in particular using AMT SOL) 
        
    g_uart_config = rpb->uart_config;
    g_uart_config.fifo = 0; // FIXME: work-around for issue #143 
    init_uart();
    printf("\nrpb->uart_config.port = %x", rpb->uart_config.port);
    printf("\nrpb->uart_config.clock_hz = %u", rpb->uart_config.clock_hz);
    printf("\nrpb->uart_config.baud = %u", rpb->uart_config.baud);
    printf("\nrpb->uart_config.data_bits, parity, stop_bits, fifo = %x %x %x %x", 
		rpb->uart_config.data_bits, rpb->uart_config.parity, rpb->uart_config.stop_bits, rpb->uart_config.fifo);
#endif*/
	emhf_debug_init((char *)&rpb->uart_config);
	printf("\nruntime initializing...");

	//initialize basic platform elements
	emhf_baseplatform_initialize();


    //[debug] dump E820 and MP table
 	printf("\nNumber of E820 entries = %u", rpb->XtVmmE820NumEntries);
	{
		int i;
		for(i=0; i < (int)rpb->XtVmmE820NumEntries; i++){
		printf("\n0x%08x%08x, size=0x%08x%08x (%u)", 
          g_e820map[i].baseaddr_high, g_e820map[i].baseaddr_low,
          g_e820map[i].length_high, g_e820map[i].length_low,
          g_e820map[i].type);
		}
  	}
	printf("\nNumber of MP entries = %u", rpb->XtVmmMPCpuinfoNumEntries);
	{
		int i;
		for(i=0; i < (int)rpb->XtVmmMPCpuinfoNumEntries; i++)
			printf("\nCPU #%u: bsp=%u, lapic_id=0x%02x", i, g_cpumap[i].isbsp, g_cpumap[i].lapic_id);
	}


	//setup EMHF exception handler component
	emhf_xcphandler_initialize();

	//[debug]: test IDT/exception routing
	//__asm__ __volatile__ ("int $0x03\r\n");



#if defined (__DMAPROT__)
	{
			u64 protectedbuffer_paddr;
			u32 protectedbuffer_vaddr;
			u32 protectedbuffer_size;
			
			if(cpu_vendor == CPU_VENDOR_AMD){
				protectedbuffer_paddr = __hva2spa__(&g_svm_dev_bitmap);
				protectedbuffer_vaddr = (u32)&g_svm_dev_bitmap;
				protectedbuffer_size = 131072; //sizeof(g_svm_dev_bitmap) XXX: TODO remove hard-coded constant
			}else{	//CPU_VENDOR_INTEL
				protectedbuffer_paddr = __hva2spa__(&g_vmx_vtd_pdp_table);
				protectedbuffer_vaddr = (u32)&g_vmx_vtd_pdp_table;
				protectedbuffer_size = PAGE_SIZE_4K + (PAGE_SIZE_4K * PAE_PTRS_PER_PDPT) 
					+ (PAGE_SIZE_4K * PAE_PTRS_PER_PDPT * PAE_PTRS_PER_PDT) + PAGE_SIZE_4K +
					(PAGE_SIZE_4K * PCI_BUS_MAX);
			}
	
			printf("\nRuntime: Re-initializing DMA protection...");
			if(!emhf_dmaprot_initialize(protectedbuffer_paddr, protectedbuffer_vaddr, protectedbuffer_size)){
				printf("\nRuntime: Unable to re-initialize DMA protection. HALT!");
				HALT();
			}


			//protect SL and runtime memory regions
			emhf_dmaprot_protect(rpb->XtVmmRuntimePhysBase - PAGE_SIZE_2M, rpb->XtVmmRuntimeSize+PAGE_SIZE_2M);
			printf("\nRuntime: Protected SL+Runtime (%08lx-%08x) from DMA.", 
				rpb->XtVmmRuntimePhysBase - PAGE_SIZE_2M,
				rpb->XtVmmRuntimePhysBase+rpb->XtVmmRuntimeSize);
	}
#endif //__DMAPROT__

	//initialize base platform with SMP 
	emhf_baseplatform_smpinitialize();
	
	printf("\nRuntime: We should NEVER get here!");
	ASSERT(0);
	HALT();
}

//we get control here in the context of *each* physical CPU core 
//vcpu->isbsp = 1 if the core is a BSP or 0 if its an AP
//isEarlyInit = 1 if we were boot-strapped by the BIOS and is 0
//in the event we were launched from a running OS
void emhf_runtime_main(VCPU *vcpu, u32 isEarlyInit){
  //initialize CPU
  emhf_baseplatform_cpuinitialize();

  //initialize partition monitor (i.e., hypervisor) for this CPU
  emhf_partition_initializemonitor(vcpu);

  //setup guest OS state for partition
  emhf_partition_setupguestOSstate(vcpu);

  //initialize memory protection for this core
  emhf_memprot_initialize(vcpu);

  //initialize application parameter block and call app main
  {
  	APP_PARAM_BLOCK appParamBlock;
  	
		appParamBlock.bootsector_ptr = (u32)rpb->XtGuestOSBootModuleBase;
  	appParamBlock.bootsector_size = (u32)rpb->XtGuestOSBootModuleSize;
  	appParamBlock.optionalmodule_ptr = (u32)rpb->XtGuestOSBootModuleBaseSup1;
  	appParamBlock.optionalmodule_size = (u32)rpb->XtGuestOSBootModuleSizeSup1;
 		appParamBlock.runtimephysmembase = (u32)rpb->XtVmmRuntimePhysBase;  

  	//call app main
  	if(emhf_app_main(vcpu, &appParamBlock)){
    	printf("\nCPU(0x%02x): EMHF app. failed to initialize. HALT!", vcpu->id);
    	HALT();
  	}
  }   	

  //increment app main success counter
  spin_lock(&g_lock_appmain_success_counter);
  g_appmain_success_counter++;
  spin_unlock(&g_lock_appmain_success_counter);
	
  //if BSP, wait for all cores to go through app main successfully
  //TODO: conceal g_midtable_numentries behind interface
  //emhf_baseplatform_getnumberofcpus
  if(vcpu->isbsp && (g_midtable_numentries > 1)){
		printf("\nCPU(0x%02x): Waiting for all cores to cycle through appmain...", vcpu->id);
		while(g_appmain_success_counter < g_midtable_numentries);	
		printf("\nCPU(0x%02x): All cores have successfully been through appmain.", vcpu->id);
  }

	//late initialization is still WiP and we can get only this far 
	//currently
	if(!isEarlyInit){
		printf("\nCPU(0x%02x): Late-initialization, WiP, HALT!", vcpu->id);
		HALT();
	}


  //initialize support for SMP guests
  emhf_smpguest_initialize(vcpu);

	
  //start partition
  emhf_partition_start(vcpu);

  printf("\nCPU(0x%02x): FATAL, should not be here. HALTING!", vcpu->id);
  HALT();

}

