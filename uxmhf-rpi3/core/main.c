#include <types.h>
#include <arm8-32.h>
#include <bcm2837.h>
#include <miniuart.h>
#include <atags.h>
#include <debug.h>


extern void chainload_os(u32 r0, u32 id, struct atag *at);
extern void cpumodeswitch_hyp2svc(u32 address);
extern void entry_svc(void);

extern u32 g_hypvtable[];

u32 guestos_boot_r0=0;
u32 guestos_boot_r1=0;
u32 guestos_boot_r2=0;


void hyphvc_handler(void){
	bcm2837_miniuart_puts("uXMHF-rpi3: core: hyphvc_handler [IN]\n");
	bcm2837_miniuart_puts("uXMHF-rpi3: core: Hello world from hypercall\n");
	bcm2837_miniuart_puts("uXMHF-rpi3: core: hyphvc_handler [OUT]\n");
}

void hypsvc_handler(void){
	//bcm2837_miniuart_puts("uXMHF-rpi3: core: hypSVC_handler [IN]\n");
	//bcm2837_miniuart_puts("uXMHF-rpi3: core: hypSVC_handler [OUT]\n");
}


void main_svc(void){
	u32 cpsr;

	bcm2837_miniuart_puts("uXMHF-rpi3: core: now in SVC mode\n");

	cpsr = sysreg_read_cpsr();
	bcm2837_miniuart_puts(" CPSR[mode]= ");
	debug_hexdumpu32((cpsr & 0xF));

	bcm2837_miniuart_puts("uxmhf-rpi3: core: proceeding to test hypercall (HVC) in SVC mode...\n");
	hypcall();
	bcm2837_miniuart_puts("uxmhf-rpi3: core: successful return after hypercall test.\n");

	bcm2837_miniuart_puts("uXMHF-rpi3: core: Chainloading OS kernel...\n");

	bcm2837_miniuart_flush();
	chainload_os(guestos_boot_r0, guestos_boot_r1, guestos_boot_r2);

	bcm2837_miniuart_puts("uxmhf-rpi3: core: Halting!\n");
	HALT();

}

void main(u32 r0, u32 id, struct atag *at){
	//struct atag *pat;
	//bcm2837_miniuart_init();
	//u32 cpsr;
	u32 hvbar, hcr, spsr_hyp;
	//u32 hsctlr;

	bcm2837_miniuart_puts("uXMHF-rpi3: core: Hello World!\n");
	bcm2837_miniuart_puts(" r0= ");
	debug_hexdumpu32(r0);
	bcm2837_miniuart_puts(" id= ");
	debug_hexdumpu32(id);
	bcm2837_miniuart_puts(" ATAGS= ");
	debug_hexdumpu32(at);

	if(at->size == 0xedfe0dd0)
		bcm2837_miniuart_puts("uXMHF-rpi3: core: ATAGS pointer is a FDT blob so no worries\n");
	else{
		bcm2837_miniuart_puts("uXMHF-rpi3: core: Error: require ATAGS to be FDT blob. Halting!\n");
	}

	//store guest OS boot register values
	guestos_boot_r0=r0;
	guestos_boot_r1=id;
	guestos_boot_r2=at;




	hvbar = sysreg_read_hvbar();
	bcm2837_miniuart_puts(" HVBAR before= ");
	debug_hexdumpu32(hvbar);

	bcm2837_miniuart_puts(" g_hypvtable at= ");
	debug_hexdumpu32((u32)&g_hypvtable);
	sysreg_write_hvbar((u32)&g_hypvtable);

	hvbar = sysreg_read_hvbar();
	bcm2837_miniuart_puts(" loaded HVBAR with g_hypvtable; HVBAR after= ");
	debug_hexdumpu32(hvbar);

	bcm2837_miniuart_puts("uxmhf-rpi3: core: proceeding to test hypercall (HVC) in HYP mode...\n");
	hypcall();
	bcm2837_miniuart_puts("uxmhf-rpi3: core: successful return after hypercall test.\n");


	hcr = sysreg_read_hcr();
	bcm2837_miniuart_puts(" HCR before= ");
	debug_hexdumpu32(hcr);

	spsr_hyp = sysreg_read_spsr_hyp();
	bcm2837_miniuart_puts(" SPSR_hyp= ");
	debug_hexdumpu32(spsr_hyp);

	/* initialize cpu support for second stage page table translations */
	bcm2837_miniuart_puts("uxmhf-rpi3: core: initializing cpu support for stage-2 pts...\n");
	s2pgtbl_initialize();
	bcm2837_miniuart_puts("uxmhf-rpi3: core: cpu ready for stage-2 pts...\n");

	bcm2837_miniuart_puts("uxmhf-rpi3: core: proceeding to switch to SVC mode...\n");
	cpumodeswitch_hyp2svc(&entry_svc);

	/*

	bcm2837_miniuart_puts("uXMHF-rpi3: core: Chainloading OS kernel...\n");

	bcm2837_miniuart_flush();
	chainload_os(guestos_boot_r0, guestos_boot_r1, guestos_boot_r2);
	*/

	bcm2837_miniuart_puts("uxmhf-rpi3: core: We were not supposed to be here.Halting!\n");
	HALT();

}


