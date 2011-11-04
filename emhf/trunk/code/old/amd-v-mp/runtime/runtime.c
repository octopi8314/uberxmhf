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
#include <target.h>
#include <processor.h>
#include <globals.h>

//---notes
// CLGI/STGI
// GIF is set to 1 always when reset and SVM first enabled
// if you send an NMI when GIF=0, it is held pending until GIF=1 again

// so we set GIF=1 on all cores and NMI intercept as well
// when we get the VMEXIT_NMI, we do a simple trick
// CLGI followed by STGI this will make GIF=0 and then GIF=1 which will
// deliver the pending NMI to the current IDT whichi will xfer control
// to the exception handler within the hypervisor where we quiesce.
// upon resuming the hypervisor or guest resumes normally!

//---globals and externs--------------------------------------------------------
// XXX TODO: Move these into globals.h
extern u32 _xtlpb[];
extern u32 __grube820buffer[], __mp_cpuinfo[], __midtable[];
extern u32 svm_iopm[];
extern u32 svm_msrpm[];

PXTLPB	lpb=(PXTLPB)_xtlpb;
GRUBE820 *grube820list = (GRUBE820 *)__grube820buffer;
PCPU *pcpus= (PCPU *)__mp_cpuinfo;

MIDTAB *midtable = (MIDTAB *)__midtable;

RUNTIME_GLOBALS g_runtime; // from globals.h
                    

//---forward declarations-------------------------------------------------------
void cstartup(void);
u32 isbsp(void);


//---IOPM Bitmap initialization routine-----------------------------------------
void initIOinterception(VCPU *vcpu, struct vmcb_struct *vmcb){
  //clear bitmap buffer
  memset((void *)svm_iopm, 0, SIZEOF_IOPM_BITMAP);
  
  //setup default intercept for PCI data port
  sechyp_iopm_set_write(vcpu, PCI_CONFIG_DATA_PORT, 4);
  
  //setup vmcb iopm
  vmcb->iopm_base_pa = __hva2spa__((u32)svm_iopm);
}   

//---MSRPM Bitmap initialization routine----------------------------------------
void initMSRinterception(VCPU *vcpu, struct vmcb_struct *vmcb){
  //clear bitmap buffer
  memset((void *)svm_msrpm, 0, SIZEOF_MSRPM_BITMAP);
  
  //[test]setup default intercept for MSR_LAPIC
  sechyp_msrpm_set_write(vcpu, MSR_EFER);
    
  //setup vmcb msrpm
  vmcb->msrpm_base_pa = __hva2spa__((u32)svm_msrpm);
}   


//---function to obtain the vcpu of the currently executing core----------------
//note: this always returns a valid VCPU pointer
VCPU *getvcpu(void){
  int i;
  u32 eax, edx, *lapic_reg;
  u32 lapic_id;
  
  //read LAPIC id of this core
  rdmsr(MSR_APIC_BASE, &eax, &edx);
  ASSERT( edx == 0 ); //APIC is below 4G
  eax &= (u32)0xFFFFF000UL;
  lapic_reg = (u32 *)((u32)eax+ (u32)LAPIC_ID);
  lapic_id = *lapic_reg;
  //printf("\n%s: lapic base=0x%08x, id reg=0x%08x", __FUNCTION__, eax, lapic_id);
  lapic_id = lapic_id >> 24;
  //printf("\n%s: lapic_id of core=0x%02x", __FUNCTION__, lapic_id);
  
  for(i=0; i < (int)g_runtime.midtable_numentries; i++){
    if(midtable[i].cpu_lapic_id == lapic_id)
        return( (VCPU *)midtable[i].vcpu_vaddr_ptr );
  }

  printf("\n%s: fatal, unable to retrieve vcpu for id=0x%02x", __FUNCTION__, lapic_id);
  HALT();
}



//---microsecond delay----------------------------------------------------------
void udelay(u32 usecs){
  u8 val;
  u32 latchregval;  

  //enable 8254 ch-2 counter
  val = inb(0x61);
  val &= 0x0d; //turn PC speaker off
  val |= 0x01; //turn on ch-2
  outb(val, 0x61);
  
  //program ch-2 as one-shot
  outb(0xB0, 0x43);
  
  //compute appropriate latch register value depending on usecs
  latchregval = (1193182 * usecs) / 1000000;

  //write latch register to ch-2
  val = (u8)latchregval;
  outb(val, 0x42);
  val = (u8)((u32)latchregval >> (u32)8);
  outb(val , 0x42);
  
  //wait for countdown
  while(!(inb(0x61) & 0x20));
  
  //disable ch-2 counter
  val = inb(0x61);
  val &= 0x0c;
  outb(val, 0x61);
}

// layer of indirection for flag variable
u32 is_drtm_complete(void) {
    printf("\ng_runtime.skinit_status_flag %d", g_runtime.skinit_status_flag);
    return g_runtime.skinit_status_flag;
}

// XXX TODO: integrate this function and wakeupAPs() to avoid
// duplication of code. -JMM
void send_init_ipi_to_all_APs(void) {
  u32 eax, edx;
  volatile u32 *icr;
  
  //read LAPIC base address from MSR
  rdmsr(MSR_APIC_BASE, &eax, &edx);
  ASSERT( edx == 0 ); //APIC is below 4G
  printf("\nLAPIC base and status=0x%08x", eax);
    
  icr = (u32 *) (((u32)eax & 0xFFFFF000UL) + 0x300);

  //our test code is at 1000:0000, we need to send 10 as vector
  //send INIT
  printf("\nSending INIT IPI to all APs...");
  *icr = 0x000c4500UL;
  udelay(10000);
  //wait for command completion
  {
    u32 val;
    do{
      val = *icr;
    }while( (val & 0x1000) );
  }
  printf("Done.");
}

//---wakeupAPs------------------------------------------------------------------
// This function is called twice.  Once before DRTM is established, and once
// afterwards.  On AMD, the APs need to be in the INIT state with their microcode cleared.

void wakeupAPs(void){
  u32 eax, edx;
  volatile u32 *icr;
  
  //read LAPIC base address from MSR
  rdmsr(MSR_APIC_BASE, &eax, &edx);
  ASSERT( edx == 0 ); //APIC is below 4G
  printf("\nLAPIC base and status=0x%08x", eax);
    
  icr = (u32 *) (((u32)eax & 0xFFFFF000UL) + 0x300);
    
  {
    //extern u32 ap_code_start[], ap_code_end[];
    //memcpy(0x10000, (void *)ap_code_start, (u32)ap_code_end - (u32)ap_code_start + 1);
    extern u32 _ap_bootstrap_start[], _ap_bootstrap_end[];
    extern u32 _ap_cr3_value, _ap_cr4_value;
    _ap_cr3_value = read_cr3();
    _ap_cr4_value = read_cr4();
    memcpy((void *)0x10000, (void *)_ap_bootstrap_start, (u32)_ap_bootstrap_end - (u32)_ap_bootstrap_start + 1);
  
  }

  //our test code is at 1000:0000, we need to send 10 as vector
  //send INIT
  printf("\nSending INIT IPI to all APs...");
  *icr = 0x000c4500UL;
  udelay(10000);
  //wait for command completion
  {
    u32 val;
    do{
      val = *icr;
    }while( (val & 0x1000) );
  }
  printf("Done.");

  //send SIPI (twice as per the MP protocol)
  {
    int i;
    for(i=0; i < 2; i++){
      printf("\nSending SIPI-%u...", i);
      *icr = 0x000c4610UL;
      udelay(200);
        //wait for command completion
        {
          u32 val;
          do{
            val = *icr;
          }while( (val & 0x1000) );
        }
        printf("Done.");
      }
  }    
    
  printf("\nAPs should be awake!");
}

void dump_bytes(char *label, unsigned char *bytes, unsigned int len) {
  unsigned int i;
  if(!bytes) return;
  if(label) printf("\n%s (%d bytes):\n", label, len);

  for (i=0; i<len; i++) {
    printf("%02x", bytes[i]);
    if(i>0 && !((i+1)%16)) {
      printf("\n");
    } else {
      printf(" ");
    }
  }
  if(len%16) {
    printf("\n");
  }
}

extern u32 uart_tx_empty(void);

void print_affected_flags(void) {
    u32 eflags, efer, vm_cr, dummy;    
    
    get_eflags(eflags);
    rdmsr((u32)MSR_EFER, &efer, &dummy);
    rdmsr((u32)VM_CR_MSR, &vm_cr, &dummy);

    printf("\nFLAG: eflags 0x%08lx", eflags);
    printf("\nFLAG: efer 0x%08lx", efer);
    printf("\nFLAG: vm_cr 0x%08lx", vm_cr);          
}

void do_drtm(void) {
    int i;
    void *slb_region;
    // defined in slb.S.  XXX TODO Move extern declarations into globals.h
    extern u32 _slb_bootstrap_start[], _slb_bootstrap_end[],
        _slb_start[], _slb_end[],
        _slb_cr3_value[],
        _slb_cr4_value[],
        _slb_esp_value[],
        _slb_ebp_value[],
        _slb_post_skinit_entry[];
    
    printf("\nglobal _slb_start 0x%08lx",             (unsigned int)_slb_start);
    printf("\nglobal _slb_bootstrap_start 0x%08lx",   (unsigned int)_slb_bootstrap_start);
    printf("\nglobal _slb_cr3_value 0x%08lx",         (unsigned int)_slb_cr3_value);
    printf("\nglobal _slb_cr4_value 0x%08lx",         (unsigned int)_slb_cr4_value);
    printf("\nglobal _slb_esp_value 0x%08lx",         (unsigned int)_slb_esp_value);
    printf("\nglobal _slb_ebp_value 0x%08lx",         (unsigned int)_slb_ebp_value);
    printf("\nglobal _slb_post_skinit_entry 0x%08lx", (unsigned int)_slb_post_skinit_entry);
    printf("\nglobal _slb_bootstrap_end 0x%08lx",     (unsigned int)_slb_bootstrap_end);
    printf("\nglobal _slb_end 0x%08lx",               (unsigned int)_slb_end);

    _slb_cr3_value[0] = read_cr3();
    _slb_cr4_value[0] = read_cr4();
    _slb_esp_value[0] = read_esp() & 0xfffff000; // mask stack; cheap way to read it
    _slb_ebp_value[0] = read_ebp() & 0xfffff000; // mask base; cheap way to read it
    _slb_post_skinit_entry[0] = (u32)cstartup;    
    
    printf("\n_slb_cr3_value[0] 0x%08lx", _slb_cr3_value[0]);
    printf("\n_slb_cr4_value[0] 0x%08lx", _slb_cr4_value[0]);
    printf("\n_slb_esp_value[0] 0x%08lx", _slb_esp_value[0]);
    printf("\n_slb_ebp_value[0] 0x%08lx", _slb_ebp_value[0]);
    printf("\n_slb_post_skinit_entry[0] 0x%08lx", _slb_post_skinit_entry[0]);

    slb_region = (void *)SLB_BOOTSTRAP_CODE_BASE; // 1GB
    memset(slb_region, 0, 0x10000); // zero 64 KB
    memcpy(slb_region, _slb_start, (unsigned int)_slb_end - (unsigned int)_slb_start);    
    
    // dump SLB's interesting contents for verification
    //dump_bytes("slb.S", (unsigned char *)_slb_start, (unsigned int)_slb_end-(unsigned int)_slb_start);

    // APs need to be in INIT state to give skinit best chance to complete successfully
    send_init_ipi_to_all_APs();

    // Global variables that need to be re-initialized
    // XXX TODO: group global variables into a nice struct and init the whole thing cleanly

    init_runtime_globals(&g_runtime);
    init_islayer_globals(&g_islayer);
    g_runtime.skinit_status_flag = 1; // skinit not technically done yet, but we're at the point of no return
    
    call_skinit((unsigned long)slb_region); // we will end up back in cstartup()!
/*     printf("\nFAKING SKINIT!!! Resetting esp/ebp and calling cstartup()\n"); */
/*     __asm__ __volatile__("mov %0, %%esp":: "r"(_slb_esp_value[0])); */
/*     __asm__ __volatile__("mov %0, %%ebp":: "r"(_slb_ebp_value[0])); */
/*     cstartup(); */
}

#ifdef __NESTED_PAGING__
//---npt initialize-------------------------------------------------------------
void nptinitialize(u32 npt_pdpt_base, u32 npt_pdts_base, u32 npt_pts_base){
	pdpt_t pdpt;
	pdt_t pdts, pdt;
	pt_t pt;
	u32 paddr=0, i, j, k, y, z;
	u64 flags;
	
	printf("\n%s: pdpt=0x%08x, pdts=0x%08x, pts=0x%08x",
    __FUNCTION__, npt_pdpt_base, npt_pdts_base, npt_pts_base);
	
	pdpt=(pdpt_t)npt_pdpt_base;

  for(i = 0; i < PAE_PTRS_PER_PDPT; i++){
    y = (u32)__hva2spa__((u32)npt_pdts_base + (i << PAGE_SHIFT_4K));
    flags = (u64)(_PAGE_PRESENT);
		pdpt[i] = pae_make_pdpe((u64)y, flags);
    pdt=(pdt_t)((u32)npt_pdts_base + (i << PAGE_SHIFT_4K));
	       	
		for(j=0; j < PAE_PTRS_PER_PDT; j++){
			z=(u32)__hva2spa__((u32)npt_pts_base + ((i * PAE_PTRS_PER_PDT + j) << (PAGE_SHIFT_4K)));
		  flags = (u64)(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER);
			pdt[j] = pae_make_pde((u64)z, flags);
			pt=(pt_t)((u32)npt_pts_base + ((i * PAE_PTRS_PER_PDT + j) << (PAGE_SHIFT_4K)));
			
			for(k=0; k < PAE_PTRS_PER_PT; k++){
        flags = (u64)(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER);
        pt[k] = pae_make_pte((u64)paddr, flags);
				paddr+= PAGE_SIZE_4K;
			}
		}
  }
}
#endif


//---setup vcpu structures for all the cores including BSP----------------------
// XXX TODO midtable_numentries is a parameter to this function,
// even though it is a global. Need to clean that up.
void setupvcpus(MIDTAB *midtable, u32 midtable_numentries){
  u32 i;
  extern u32 __cpustacks[], __vcpubuffers[];
  extern u32 svm_hsave_buffers[], svm_vmcb_buffers[];
  extern u32 svm_npt_pdpt_buffers[], svm_npt_pdts_buffers[], svm_sipi_page_buffers[];
  extern u32 svm_npt_pts_buffers[];
  
  
#ifdef __NESTED_PAGING__
  u32 npt_current_asid=ASID_GUEST_KERNEL;
#endif
  
  VCPU *vcpu;
  
  printf("\n%s: __cpustacks range 0x%08x-0x%08x in 0x%08x chunks",
    __FUNCTION__, (u32)__cpustacks, (u32)__cpustacks + (RUNTIME_STACK_SIZE * MAX_VCPU_ENTRIES),
        RUNTIME_STACK_SIZE);
  printf("\n%s: __vcpubuffers range 0x%08x-0x%08x in 0x%08x chunks",
    __FUNCTION__, (u32)__vcpubuffers, (u32)__vcpubuffers + (SIZE_STRUCT_VCPU * MAX_VCPU_ENTRIES),
        SIZE_STRUCT_VCPU);
  printf("\n%s: svm_hsave_buffers range 0x%08x-0x%08x in 0x%08x chunks",
    __FUNCTION__, (u32)svm_hsave_buffers, (u32)svm_hsave_buffers + (8192 * MAX_VCPU_ENTRIES),
        8192);
  printf("\n%s: svm_vmcb_buffers range 0x%08x-0x%08x in 0x%08x chunks",
    __FUNCTION__, (u32)svm_vmcb_buffers, (u32)svm_vmcb_buffers + (8192 * MAX_VCPU_ENTRIES),
        8192);

#ifdef __NESTED_PAGING__
    printf("\n%s: svm_npt_pdpt_buffers range 0x%08x-0x%08x in 0x%08x chunks",
      __FUNCTION__, (u32)svm_npt_pdpt_buffers, (u32)svm_npt_pdpt_buffers + (4096 * MAX_VCPU_ENTRIES),
          4096);
    printf("\n%s: svm_npt_pdts_buffers range 0x%08x-0x%08x in 0x%08x chunks",
      __FUNCTION__, (u32)svm_npt_pdts_buffers, (u32)svm_npt_pdts_buffers + (16384 * MAX_VCPU_ENTRIES),
          16384);
    printf("\n%s: svm_npt_pts_buffers range 0x%08x-0x%08x in 0x%08x chunks",
      __FUNCTION__, (u32)svm_npt_pts_buffers, (u32)svm_npt_pts_buffers + ((2048*4096) * MAX_VCPU_ENTRIES),
          (2048*4096));
#endif
          
  for(i=0; i < midtable_numentries; i++){
    vcpu = (VCPU *)((u32)__vcpubuffers + (u32)(i * SIZE_STRUCT_VCPU));
    memset((void *)vcpu, 0, sizeof(VCPU));
    
    vcpu->esp = ((u32)__cpustacks + (i * RUNTIME_STACK_SIZE)) + RUNTIME_STACK_SIZE;    
    vcpu->hsave_vaddr_ptr = ((u32)svm_hsave_buffers + (i * 8192));
    vcpu->vmcb_vaddr_ptr = ((u32)svm_vmcb_buffers + (i * 8192));

#ifdef __NESTED_PAGING__
    {
      u32 npt_pdpt_base, npt_pdts_base, npt_pts_base;
      npt_pdpt_base = ((u32)svm_npt_pdpt_buffers + (i * 4096)); 
      npt_pdts_base = ((u32)svm_npt_pdts_buffers + (i * 16384));
      npt_pts_base = ((u32)svm_npt_pts_buffers + (i * (2048*4096)));
      nptinitialize(npt_pdpt_base, npt_pdts_base, npt_pts_base);
      vcpu->npt_vaddr_ptr = npt_pdpt_base;
      vcpu->npt_vaddr_pts = npt_pts_base;
      vcpu->npt_asid = npt_current_asid;
      npt_current_asid++;
    }
#endif
    
    vcpu->id = midtable[i].cpu_lapic_id;
    vcpu->sipivector = 0;
    vcpu->sipireceived = 0;

    midtable[i].vcpu_vaddr_ptr = (u32)vcpu;
    printf("\nCPU #%u: vcpu_vaddr_ptr=0x%08x, esp=0x%08x", i, midtable[i].vcpu_vaddr_ptr,
      vcpu->esp);
    printf("\n  hsave_vaddr_ptr=0x%08x, vmcb_vaddr_ptr=0x%08x", vcpu->hsave_vaddr_ptr,
          vcpu->vmcb_vaddr_ptr);
  }
}


//---runtime main---------------------------------------------------------------
void cstartup(void){
	//setup debugging	
#ifdef __DEBUG_SERIAL__	
	init_uart();
#endif
	printf("\nruntime initializing...");
    if(is_drtm_complete()) {
        u32 eax, edx;
        
        init_runtime_globals(&g_runtime);
        g_runtime.skinit_status_flag = 1; // want to preserve skinit_status_flag

        // need to clear some bits in VM_CR that were set by SKINIT
        rdmsr((u32)VM_CR_MSR, &eax, &edx);
        eax &= (~(1<<VM_CR_DPD)); // Clear DPD
        eax &= (~(1<<VM_CR_R_INIT)); // Clear R_INIT
        eax &= (~(1<<VM_CR_DIS_A20M)); // Clear DIS_A20M
        wrmsr((u32)VM_CR_MSR, eax, edx);
    } else {
        init_runtime_globals(&g_runtime);
    }
    init_islayer_globals(&g_islayer);
    print_affected_flags();

  //debug, dump E820 and MP table
 	printf("\nNumber of E820 entries = %u", lpb->XtVmmE820NumEntries);
  {
    int i;
    for(i=0; i < (int)lpb->XtVmmE820NumEntries; i++){
      printf("\n0x%08x%08x, size=0x%08x%08x (%u)", 
          grube820list[i].baseaddr_high, grube820list[i].baseaddr_low,
          grube820list[i].length_high, grube820list[i].length_low,
          grube820list[i].type);
    }
  
  }
  printf("\nNumber of MP entries = %u", lpb->XtVmmMPCpuinfoNumEntries);
  {
    int i;
    for(i=0; i < (int)lpb->XtVmmMPCpuinfoNumEntries; i++)
      printf("\nCPU #%u: bsp=%u, lapic_id=0x%02x", i, pcpus[i].isbsp, pcpus[i].lapic_id);
  }

  //setup Master-ID Table (MIDTABLE)
  {
    int i;
    for(i=0; i < (int)lpb->XtVmmMPCpuinfoNumEntries; i++){
       midtable[g_runtime.midtable_numentries].cpu_lapic_id = pcpus[i].lapic_id;
       midtable[g_runtime.midtable_numentries].vcpu_vaddr_ptr = 0;
       g_runtime.midtable_numentries++;
    }
  }

  //setup vcpus
  setupvcpus(midtable, g_runtime.midtable_numentries);

  /* Possible alternative location for microcode clear; prefer allcpus_common_start. */
/*   //inserted by Jon to clear BSP microcode */
/*   //AP microcode cleared during bootup in runtimesup.S */
/*   { */
/*     int dummy=0; */
/*     wrmsr(MSR_AMD64_PATCH_CLEAR, dummy, dummy); */
/*   } */

  
  
  //wake up APS
  if(g_runtime.midtable_numentries > 1)
    wakeupAPs();

  //fall through to common code  
  {
	 void _ap_pmode_entry_with_paging(void);
   printf("\nRelinquishing BSP thread and moving to common...");
   _ap_pmode_entry_with_paging();
   printf("\nBSP must never get here. HALT!");
   HALT();
  }
}


//---isbsp----------------------------------------------------------------------
//returns 1 if the calling CPU is the BSP, else 0
u32 isbsp(void){
  u32 eax, edx;
  //read LAPIC base address from MSR
  rdmsr(MSR_APIC_BASE, &eax, &edx);
  ASSERT( edx == 0 ); //APIC is below 4G
  
  if(eax & 0x100)
    return 1;
  else
    return 0;
}

//---setup VMCB-----------------------------------------------------------------
void initVMCB(VCPU *vcpu){
  
  struct vmcb_struct *vmcb = (struct vmcb_struct *)vcpu->vmcb_vaddr_ptr;
  
  printf("\nCPU(0x%02x): VMCB at 0x%08x", vcpu->id, (u32)vmcb);
  memset(vmcb, 0, sizeof(struct vmcb_struct));
  
  // set up CS descr 
  vmcb->cs.sel = 0x0;
  vmcb->cs.base = 0x0;
  vmcb->cs.limit = 0x0ffff; // 64K limit since g=0 
  vmcb->cs.attr.bytes = 0x009b;
  
  // set up DS descr 
  vmcb->ds.sel = 0x0;
  vmcb->ds.base = 0x0;
  vmcb->ds.limit = 0x0ffff; // 64K limit since g=0 
  vmcb->ds.attr.bytes = 0x0093; // read/write 
  
  // set up ES descr 
  vmcb->es.sel = 0x0;
  vmcb->es.base = 0x0;
  vmcb->es.limit = 0x0ffff; // 64K limit since g=0 
  vmcb->es.attr.bytes = 0x0093; // read/write 

  // set up FS descr 
  vmcb->fs.sel = 0x0;
  vmcb->fs.base = 0x0;
  vmcb->fs.limit = 0x0ffff; // 64K limit since g=0 
  vmcb->fs.attr.bytes = 0x0093; // read/write 

  // set up GS descr 
  vmcb->gs.sel = 0x0;
  vmcb->gs.base = 0x0;
  vmcb->gs.limit = 0x0ffff; // 64K limit since g=0 
  vmcb->gs.attr.bytes = 0x0093; // read/write 

  // set up SS descr 
  vmcb->ss.sel = 0x0;
  vmcb->ss.base = 0x0;
  vmcb->ss.limit = 0x0ffff; // 64K limit since g=0 
  vmcb->ss.attr.bytes = 0x0093; // read/write 

  vmcb->idtr.limit = 0x03ff;

  // SVME needs to be set in EFER for vmrun to execute 
  vmcb->efer |= (1 << EFER_SVME);
   
  // set guest PAT to state at reset. 
  vmcb->g_pat = 0x0007040600070406ULL;

  // other-non general purpose registers/state 
  vmcb->guest_asid = 1; // ASID 0 reserved for host 
  vmcb->cpl = 0; // set cpl to 0 for real mode 

  // general purpose registers 
  vmcb->rax= 0x0ULL;
  vmcb->rsp= 0x0ULL;

  if(isbsp()){
    printf("\nBSP(0x%02x): copying boot-module to boot guest", vcpu->id);
  	memcpy((void *)__GUESTOSBOOTMODULE_BASE, (void *)lpb->XtGuestOSBootModuleBase, lpb->XtGuestOSBootModuleSize);
    vmcb->rip = 0x7c00ULL;
  }else{

#ifdef __NESTED_PAGING__
      vmcb->cs.sel = (vcpu->sipivector * PAGE_SIZE_4K) >> 4;
      vmcb->cs.base = (vcpu->sipivector * PAGE_SIZE_4K);
      vmcb->rip = 0x0ULL;
#else
      //poke a spin loop at 0040:00AC BDA-reserved loc
      u8 *code = (u8 *)0x4AC;
      printf("\nCPU(0x%02x): poking spin loop to start guest", vcpu->id);
      code[0]=0xEB; code[1]=0xFE;
      vmcb->rip = 0xACULL;
      vmcb->cs.sel = 0x0040;
      vmcb->cs.base = 0x400;
#endif

  }
  vmcb->rflags = 0x0ULL;
  
  vmcb->cr0 = 0x00000010ULL;
  vmcb->cr2 = 0ULL;
  vmcb->cr3 = 0x0ULL;
  vmcb->cr4 = 0ULL;
  
  vmcb->dr6 = 0xffff0ff0ULL;
  vmcb->dr7 = 0x00000400ULL;
 
  vmcb->cr_intercepts = 0;
  vmcb->dr_intercepts = 0;
  
  // intercept all SVM instructions 
  vmcb->general2_intercepts |= (u32)(GENERAL2_INTERCEPT_VMRUN |
					  GENERAL2_INTERCEPT_VMMCALL |
					  GENERAL2_INTERCEPT_VMLOAD |
					  GENERAL2_INTERCEPT_VMSAVE |
					  GENERAL2_INTERCEPT_STGI |
					  GENERAL2_INTERCEPT_CLGI |
					  GENERAL2_INTERCEPT_SKINIT |
					  GENERAL2_INTERCEPT_ICEBP);

#ifdef __NESTED_PAGING__
	vmcb->h_cr3 = __hva2spa__(vcpu->npt_vaddr_ptr);
  vmcb->np_enable |= (1ULL << NP_ENABLE);
	vmcb->guest_asid = vcpu->npt_asid;
	printf("\nCPU(0x%02x): Activated NPTs.", vcpu->id);
#endif

  if(isbsp())
	 vmcb->general1_intercepts |= (u32) GENERAL1_INTERCEPT_SWINT;

  //intercept NMIs, required for core quiescing support
  vmcb->general1_intercepts |= (u32) GENERAL1_INTERCEPT_NMI;

  //setup IO interception
  initIOinterception(vcpu, vmcb);
  vmcb->general1_intercepts |= (u32) GENERAL1_INTERCEPT_IOIO_PROT;

  //setup MSR interception
  initMSRinterception(vcpu, vmcb);
  vmcb->general1_intercepts |= (u32) GENERAL1_INTERCEPT_MSR_PROT;


  return;
}


//---init SVM-------------------------------------------------------------------
void initSVM(VCPU *vcpu){
  u32 eax, edx, ecx, ebx;
  u64 hsave_pa;
  u32 i;

  //check if CPU supports SVM extensions 
  cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
  if( !(ecx & (1<<ECX_SVM)) ){
   printf("\nCPU(0x%02x): no SVM extensions. HALT!", vcpu->id);
   HALT();
  }
  
  //check if SVM extensions are disabled by the BIOS 
  rdmsr(VM_CR_MSR, &eax, &edx);
  if( eax & (1<<VM_CR_SVME_DISABLE) ){
    printf("\nCPU(0x%02x): SVM extensions disabled in the BIOS. HALT!", vcpu->id);
    HALT();
  }

#ifdef __NESTED_PAGING__
  // check for nested paging support and number of ASIDs 
	cpuid(0x8000000A, &eax, &ebx, &ecx, &edx);
  if(!(edx & 0x1)){
      printf("\nCPU(0x%02x): No support for Nested Paging, HALTING!", vcpu->id);
		HALT();
	}
	
  printf("\nCPU(0x%02x): Nested paging support present", vcpu->id);
	if( (ebx-1) < 2 ){
		printf("\nCPU(0x%02x): Total number of ASID is too low, HALTING!", vcpu->id);
		HALT();
	}
	
	printf("\nCPU(0x%02x): Total ASID is valid");
#endif

  // enable SVM and debugging support (if required)   
  rdmsr((u32)VM_CR_MSR, &eax, &edx);
  eax &= (~(1<<VM_CR_DPD));
  wrmsr((u32)VM_CR_MSR, eax, edx);
  printf("\nCPU(0x%02x): HDT debugging enabled", vcpu->id);

  rdmsr((u32)MSR_EFER, &eax, &edx);
  eax |= (1<<EFER_SVME);
  wrmsr((u32)MSR_EFER, eax, edx);
  printf("\nCPU(0x%02x): SVM extensions enabled", vcpu->id);

  // Initialize the HSA 
  //printf("\nHSAVE area=0x%08X", vcpu->hsave_vaddr_ptr);
  hsave_pa = __hva2spa__(vcpu->hsave_vaddr_ptr);
  //printf("\nHSAVE physaddr=0x%08x", hsave_pa);
  eax = (u32)hsave_pa;
  edx = (u32)(hsave_pa >> 32);
  wrmsr((u32)VM_HSAVE_PA, eax, edx);
  printf("\nCPU(0x%02x): SVM HSAVE initialized", vcpu->id);

  // enable NX protections 
  rdmsr(MSR_EFER, &eax, &edx);
  eax |= (1 << EFER_NXE);
  wrmsr(MSR_EFER, eax, edx);
  printf("\nCPU(0x%02x): NX protection enabled", vcpu->id);

  return;
}


//---CPUs must all have their microcode cleared for SKINIT to be successful-----
void clearMicrocode(VCPU *vcpu){
  u32 ucode_rev;
  u32 dummy=0;

  // Current microcode patch level available via MSR read

  rdmsr(MSR_AMD64_PATCH_LEVEL, &ucode_rev, &dummy);
  printf("\nCPU%d: existing microcode version 0x%08x", vcpu->id, ucode_rev);
    
  if(ucode_rev != 0) {
      wrmsr(MSR_AMD64_PATCH_CLEAR, dummy, dummy);
      printf("\nCPU%d: microcode CLEARED", vcpu->id);
  }

  spin_lock(&g_runtime.lock_cleared_ucode);
  g_runtime.cleared_ucode++;
  spin_unlock(&g_runtime.lock_cleared_ucode);

  printf("\ng_runtime.cleared_ucode now %d", g_runtime.cleared_ucode);
}


//---allcpus_common_start-------------------------------------------------------
void allcpus_common_start(VCPU *vcpu){
  //we start here with all CPUs executing common code, we 
  //will make BSP distinction based on isbsp macro which basically
  //reads the LAPIC MSR to see if it is the BSP. This function
  //executes twice: before and after SKINIT
 
  //clear microcode
  if(!is_drtm_complete()) {
    clearMicrocode(vcpu);
  }
  
  //step:1 rally all APs up, make sure all of them started, this is
  //a task for the BSP
  if(isbsp()){
    printf("\nBSP rallying APs...");
    printf("\nBSP(0x%02x): My ESP is 0x%08x", vcpu->id, vcpu->esp);

    //increment a CPU to account for the BSP
    spin_lock(&g_runtime.lock_cpus_active);
    g_runtime.cpus_active++;
    spin_unlock(&g_runtime.lock_cpus_active);

    //wait for g_runtime.cpus_active to become g_runtime.midtable_numentries -1 to indicate
    //that all APs have been successfully started
    while(g_runtime.cpus_active < g_runtime.midtable_numentries);
    
    // Need to wait until APs active to do DRTM, since it is then that the APs will
    // clear their microcode.    
    if(!is_drtm_complete()) {
        printf("\nBSP detects that DRTM has not yet been performed. Doing DRTM...");
        do_drtm(); // this function will not return
        printf("\nERROR: IMPOSSIBLE CODE PATH!");
        HALT();
    }
    
    printf("\nAPs all awake...Setting them free...");
    spin_lock(&g_runtime.lock_ap_go_signal);
    g_runtime.ap_go_signal=1;
    spin_unlock(&g_runtime.lock_ap_go_signal);

  
  }else{
    //we are an AP, so we need to simply update the AP startup counter
    //and wait until we are told to proceed
    //increment active CPUs
    spin_lock(&g_runtime.lock_cpus_active);
    g_runtime.cpus_active++;
    spin_unlock(&g_runtime.lock_cpus_active);

    // This code path is exercized twice:
    // 1. Before SKINIT, APs are initially brought online to clear their microcode.
    //    In this case, they should then halt and wait for the INIT IPI from the BSP.
    // 2. After SKINIT, they are fully initialized and the full system boot should
    //    proceed.
    
    if(!is_drtm_complete()) { // Before SKINIT, stop here, since microcode is already  
        HALT();               // cleared.  Execution will not continue past this point.
    }                         // Rather, BSP will send INIT IPI and then we will be    
                              // reinitialized by SKINIT.

    while(!g_runtime.ap_go_signal); // After SKINIT.  Just wait for the BSP to tell us all is well.
 
    printf("\nAP(0x%02x): My ESP is 0x%08x, Waiting for SIPI...", vcpu->id, vcpu->esp);

    while(!vcpu->sipireceived);
    printf("\nAP(0x%02x): SIPI signal received, vector=0x%02x", vcpu->id, vcpu->sipivector);
  }
  
  //initialize SVM
  initSVM(vcpu);
 
  //initiaize VMCB
  initVMCB(vcpu); 

  //call app main
  if(emhf_app_main(vcpu)){
    printf("\nCPU(0x%02x): Application failed to initialize. HALT!", vcpu->id);
    HALT();
  }


#ifdef __NESTED_PAGING__
    //if we are the BSP setup SIPI intercept
    if(isbsp() && (g_runtime.midtable_numentries > 1) )
      apic_setup(vcpu);
 
#endif
    
  //start HVM
  {
    struct vmcb_struct *vmcb;
    void startHVM(VCPU *vcpu, u32 vmcb_phys_addr);
    printf("\nCPU(0x%02x): Starting HVM...", vcpu->id);
    vmcb = (struct vmcb_struct *)vcpu->vmcb_vaddr_ptr;
    printf("\n  CS:EIP=0x%04x:0x%08x", (u16)vmcb->cs.sel, (u32)vmcb->rip);
    startHVM(vcpu, __hva2spa__(vcpu->vmcb_vaddr_ptr));
    printf("\nCPU(0x%02x): FATAL, should not be here. HALTING!", vcpu->id);
    HALT();
  }

}