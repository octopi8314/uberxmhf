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

// EMHF base platform component 
// declarations
// author: amit vasudevan (amitvasudevan@acm.org)

#ifndef __EMHF_BASEPLATFORM_H__
#define __EMHF_BASEPLATFORM_H__


#ifndef __ASSEMBLY__


//----------------------------------------------------------------------
//exported DATA 
//----------------------------------------------------------------------
//system e820 map
#if defined(__EMHF_VERIFICATION__)
	extern u8 g_e820map[] __attribute__(( section(".data") ));
#else
	extern GRUBE820 g_e820map[] __attribute__(( section(".data") ));
#endif //__EMHF_VERIFICATION__

//SMP CPU map; lapic id, base, ver and bsp indication for each available core
extern PCPU	g_cpumap[] __attribute__(( section(".data") ));

//runtime stacks for individual cores
extern u8 g_cpustacks[] __attribute__(( section(".stack") ));

//VCPU structure for each "guest OS" core
extern VCPU g_vcpubuffers[] __attribute__(( section(".data") ));

//master id table, contains core lapic id to VCPU mapping information
extern MIDTAB g_midtable[] __attribute__(( section(".data") ));

//number of entries in the master id table, in essence the number of 
//physical cores in the system
extern u32 g_midtable_numentries __attribute__(( section(".data") ));

//variable that is incremented by 1 by all cores that boot up, this should
//be finally equal to g_midtable_numentries at runtime which signifies
//that all physical cores have been booted up and initialized by the runtime
extern u32 g_cpus_active __attribute__(( section(".data") ));

//SMP lock for the above variable
extern u32 g_lock_cpus_active __attribute__(( section(".data") ));
    
//variable that is set to 1 by the BSP after rallying all the other cores.
//this is used by the application cores to enter the "wait-for-SIPI" state    
extern u32 g_ap_go_signal __attribute__(( section(".data") ));

//SMP lock for the above variable
extern u32 g_lock_ap_go_signal __attribute__(( section(".data") ));

//----------------------------------------------------------------------
//exported FUNCTIONS 
//----------------------------------------------------------------------

//get CPU vendor
u32 emhf_baseplatform_getcpuvendor(void);

//initialize CPU state
void emhf_baseplatform_cpuinitialize(void);

//initialize SMP
void emhf_baseplatform_smpinitialize(void);

//initialize basic platform elements
void emhf_baseplatform_initialize(void);

//reboot platform
void emhf_baseplatform_reboot(VCPU *vcpu);



/* hypervisor-virtual-address to system-physical-address. this fn is
 * used when creating the hypervisor's page tables, and hence
 * represents ground truth (assuming they haven't since been modified)
 */
static inline spa_t hva2spa(void *hva)
{
  uintptr_t hva_ui = (uintptr_t)hva;
  uintptr_t offset = rpb->XtVmmRuntimeVirtBase - rpb->XtVmmRuntimePhysBase;
  if (hva_ui >= rpb->XtVmmRuntimePhysBase && hva_ui < rpb->XtVmmRuntimePhysBase+rpb->XtVmmRuntimeSize){
    return hva_ui + offset;
  } else if (hva_ui >= rpb->XtVmmRuntimeVirtBase && hva_ui < rpb->XtVmmRuntimeVirtBase+rpb->XtVmmRuntimeSize) {
    return hva_ui - offset;
  } else {
    return hva_ui;
  }
}

static inline void * spa2hva(spa_t spa)
{
  uintptr_t offset = rpb->XtVmmRuntimeVirtBase - rpb->XtVmmRuntimePhysBase;
  if (spa >= rpb->XtVmmRuntimePhysBase && spa < rpb->XtVmmRuntimePhysBase+rpb->XtVmmRuntimeSize){
    return (void *)(uintptr_t)(spa + offset);
  } else if (spa >= rpb->XtVmmRuntimeVirtBase && spa < rpb->XtVmmRuntimeVirtBase+rpb->XtVmmRuntimeSize) {
    return (void *)(uintptr_t)(spa - offset);
  } else {
    return (void *)(uintptr_t)(spa);
  }
}

static inline spa_t gpa2spa(gpa_t gpa) { return gpa; }
static inline gpa_t spa2gpa(spa_t spa) { return spa; }
static inline void* gpa2hva(gpa_t gpa) { return spa2hva(gpa2spa(gpa)); }
static inline gpa_t hva2gpa(hva_t hva) { return spa2gpa(hva2spa(hva)); }


#endif	//__ASSEMBLY__

//bring in arch. specific declarations
#include <arch/emhf-baseplatform-arch.h>


#endif //__EMHF_BASEPLATFORM_H__
