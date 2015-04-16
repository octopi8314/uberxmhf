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

//Intel VT-d declarations/definitions
//author: amit vasudevan (amitvasudevan@acm.org)

#include <xmhf.h>
#include <xmhf-hwm.h>
#include <xmhfhw.h>
#include <xmhf-debug.h>




//==============================================================================
// local (static) variables and function definitions
//==============================================================================




//vt-d register read function
u64 _vtd_reg_read(VTD_DRHD *dmardevice, u32 reg){
    u32 regtype=VTD_REG_32BITS, regaddr=0;
    u64 retval=0;

	//obtain register type and base address
  switch(reg){
    //32-bit registers
    case  VTD_VER_REG_OFF:
    case  VTD_GCMD_REG_OFF:
    case  VTD_GSTS_REG_OFF:
    case  VTD_FSTS_REG_OFF:
    case  VTD_FECTL_REG_OFF:
    case  VTD_PMEN_REG_OFF:
    case  VTD_PLMBASE_REG_OFF:
    case  VTD_PLMLIMIT_REG_OFF:
      regtype=VTD_REG_32BITS;
      regaddr=dmardevice->regbaseaddr+reg;
      break;

    //64-bit registers
    case  VTD_CAP_REG_OFF:
    case  VTD_ECAP_REG_OFF:
    case  VTD_RTADDR_REG_OFF:
    case  VTD_CCMD_REG_OFF:
    case  VTD_PHMBASE_REG_OFF:
    case  VTD_PHMLIMIT_REG_OFF:
      regtype=VTD_REG_64BITS;
      regaddr=dmardevice->regbaseaddr+reg;
      break;

    case  VTD_IOTLB_REG_OFF:
      regtype=VTD_REG_64BITS;
      regaddr=dmardevice->iotlb_regaddr;
      break;


    case  VTD_IVA_REG_OFF:
      regtype=VTD_REG_64BITS;
      regaddr=dmardevice->iva_regaddr;
      break;


    default:
      _XDPRINTF_("%s: Halt, Unsupported register=%08x\n", __func__, reg);
      HALT();
      break;
  }

  //perform the actual read or write request
	switch(regtype){
    case VTD_REG_32BITS:	//32-bit read
      retval = xmhfhw_sysmemaccess_readu32(regaddr);
      break;

    case VTD_REG_64BITS:	//64-bit read
      retval = xmhfhw_sysmemaccess_readu64(regaddr);
      break;

    default:
     _XDPRINTF_("%s: Halt, Unsupported access width=%08x\n", __func__, regtype);
     HALT();
  }

  return retval;
}



//vt-d register write function
void _vtd_reg_write(VTD_DRHD *dmardevice, u32 reg, u64 value){
  u32 regtype=VTD_REG_32BITS, regaddr=0;

	//obtain register type and base address
  switch(reg){
    //32-bit registers
    case  VTD_VER_REG_OFF:
    case  VTD_GCMD_REG_OFF:
    case  VTD_GSTS_REG_OFF:
    case  VTD_FSTS_REG_OFF:
    case  VTD_FECTL_REG_OFF:
    case  VTD_PMEN_REG_OFF:
    case  VTD_PLMBASE_REG_OFF:
    case  VTD_PLMLIMIT_REG_OFF:
      regtype=VTD_REG_32BITS;
      regaddr=dmardevice->regbaseaddr+reg;
      break;

    //64-bit registers
    case  VTD_CAP_REG_OFF:
    case  VTD_ECAP_REG_OFF:
    case  VTD_RTADDR_REG_OFF:
    case  VTD_CCMD_REG_OFF:
    case  VTD_PHMBASE_REG_OFF:
    case  VTD_PHMLIMIT_REG_OFF:
      regtype=VTD_REG_64BITS;
      regaddr=dmardevice->regbaseaddr+reg;
      break;

    case  VTD_IOTLB_REG_OFF:
      regtype=VTD_REG_64BITS;
      regaddr=dmardevice->iotlb_regaddr;
      break;


    case  VTD_IVA_REG_OFF:
      regtype=VTD_REG_64BITS;
      regaddr=dmardevice->iva_regaddr;
      break;


    default:
      _XDPRINTF_("%s: Halt, Unsupported register=%08x\n", __func__, reg);
      HALT();
      break;
  }

  //perform the actual read or write request
	switch(regtype){
    case VTD_REG_32BITS:	//32-bit write
      xmhfhw_sysmemaccess_writeu32(regaddr, (u32)value);
      break;

    case VTD_REG_64BITS:	//64-bit write
      xmhfhw_sysmemaccess_writeu64(regaddr, value);
      break;

    default:
     _XDPRINTF_("%s: Halt, Unsupported access width=%08x\n", __func__, regtype);
     HALT();
  }

  return;
}




//initialize a given DRHD unit to meet our requirements
bool xmhfhw_platform_x86pc_vtd_drhd_initialize(VTD_DRHD *drhd){
	VTD_GCMD_REG gcmd;
	VTD_GSTS_REG gsts;
	VTD_FECTL_REG fectl;
	VTD_CAP_REG cap;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if (drhd == NULL)
		return false;

	//verify required capabilities
	{
		//_XDPRINTF_("\nVerifying DRHD capabilities...");

		//read CAP register
		unpack_VTD_CAP_REG(&cap, _vtd_reg_read(drhd, VTD_CAP_REG_OFF));

		if(! (cap.plmr && cap.phmr) ){
			//_XDPRINTF_("\n%s: Error: PLMR unsupported", __func__);
			return false;
		}

        if ( !((cap.sagaw & 0x2) || (cap.sagaw & 0x4)) ){
            //_XDPRINTF_("%s: Error: we only support 3-level or 4-level tables (%x)\n", __func__, cap.bits.sagaw);
			return false;
        }

		//_XDPRINTF_("\nDRHD unit has all required capabilities");
	}

	//setup fault logging
	//_XDPRINTF_("\nSetting DRHD Fault-reporting to NON-INTERRUPT mode...");
	{
		//read FECTL
		//  fectl.value=0;
		//fectl.value = _vtd_reg_read(drhd, VTD_FECTL_REG_OFF);
		unpack_VTD_FECTL_REG(&fectl, _vtd_reg_read(drhd, VTD_FECTL_REG_OFF));

		//set interrupt mask bit and write
		fectl.im=1;
		_vtd_reg_write(drhd, VTD_FECTL_REG_OFF, pack_VTD_FECTL_REG(&fectl) );

		//check to see if the IM bit actually stuck
		//fectl.value = _vtd_reg_read(drhd, VTD_FECTL_REG_OFF);
		unpack_VTD_FECTL_REG(&fectl, _vtd_reg_read(drhd, VTD_FECTL_REG_OFF));

		if(!fectl.im){
		  //_XDPRINTF_("\n%s: Error: Failed to set fault-reporting.", __func__);
		  return false;
		}
	}
	//_XDPRINTF_("\nDRHD Fault-reporting set to NON-INTERRUPT mode.");

	return true;
}


//invalidate DRHD caches
//note: we do global invalidation currently
//returns: true if all went well, else false
bool xmhfhw_platform_x86pc_vtd_drhd_invalidatecaches(VTD_DRHD *drhd){
	VTD_CCMD_REG ccmd;
	VTD_IOTLB_REG iotlb;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if (drhd == NULL)
		return false;

	//invalidate CET cache
  	//wait for context cache invalidation request to send
    do{
      //ccmd.value = _vtd_reg_read(drhd, VTD_CCMD_REG_OFF);
      unpack_VTD_CCMD_REG(&ccmd, _vtd_reg_read(drhd, VTD_CCMD_REG_OFF));
    }while(ccmd.icc);

	//initialize CCMD to perform a global invalidation
    //ccmd.value=0;
    memset(&ccmd, 0, sizeof(VTD_CCMD_REG));
    ccmd.cirg=1; //global invalidation
    ccmd.icc=1;  //invalidate context cache

    //perform the invalidation
    _vtd_reg_write(drhd, VTD_CCMD_REG_OFF, pack_VTD_CCMD_REG(&ccmd));

	//wait for context cache invalidation completion status
    do{
      //ccmd.value = _vtd_reg_read(drhd, VTD_CCMD_REG_OFF);
      unpack_VTD_CCMD_REG(&ccmd, _vtd_reg_read(drhd, VTD_CCMD_REG_OFF));
    }while(ccmd.icc);

	//if all went well CCMD CAIG = CCMD CIRG (i.e., actual = requested invalidation granularity)
	if(ccmd.caig != 0x1){
		//_XDPRINTF_("\n%s: Error: Invalidatation of CET failed (%u)", __func__, ccmd.bits.caig);
		return false;
	}

	//invalidate IOTLB
    //initialize IOTLB to perform a global invalidation
	//iotlb.value=0;
	memset(&iotlb, 0, sizeof(VTD_IOTLB_REG));
    iotlb.iirg=1; //global invalidation
    iotlb.ivt=1;	 //invalidate

    //perform the invalidation
	_vtd_reg_write(drhd, VTD_IOTLB_REG_OFF, pack_VTD_IOTLB_REG(&iotlb));

    //wait for the invalidation to complete
    do{
      //iotlb.value = _vtd_reg_read(drhd, VTD_IOTLB_REG_OFF);
      unpack_VTD_IOTLB_REG(&iotlb, _vtd_reg_read(drhd, VTD_IOTLB_REG_OFF));
    }while(iotlb.ivt);

    //if all went well IOTLB IAIG = IOTLB IIRG (i.e., actual = requested invalidation granularity)
	if(iotlb.iaig != 0x1){
		//_XDPRINTF_("\n%s: Error: Invalidation of IOTLB failed (%u)", __func__, iotlb.bits.iaig);
		return false;
    }

	return true;
}


//VT-d translation has 1 root entry table (RET) of 4kb
//each root entry (RE) is 128-bits which gives us 256 entries in the
//RET, each corresponding to a PCI bus num. (0-255)
//each RE points to a context entry table (CET) of 4kb
//each context entry (CE) is 128-bits which gives us 256 entries in
//the CET, accounting for 32 devices with 8 functions each as per the
//PCI spec.
//each CE points to a PDPT type paging structure for  device
bool xmhfhw_platform_x86pc_vtd_drhd_set_root_entry_table(VTD_DRHD *drhd,  u64 ret_addr){
	VTD_RTADDR_REG rtaddr;
	VTD_GCMD_REG gcmd;
	VTD_GSTS_REG gsts;
	u32 retbuffer_paddr = hva2spa((u32)ret_addr);
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if (drhd == NULL)
		return false;

	//setup DRHD RET (root-entry)
	//_XDPRINTF_("Setting up DRHD RET: Unit %u, RETaddr=%016llx, %08x...\n",
    //            drhd_handle, ret_addr, retbuffer_paddr);
	{
		//setup RTADDR with base of RET
		unpack_VTD_RTADDR_REG(&rtaddr, retbuffer_paddr);
		_vtd_reg_write(drhd, VTD_RTADDR_REG_OFF, pack_VTD_RTADDR_REG(&rtaddr));

		//latch RET address by using GCMD.SRTP
		//gcmd.value=0;
		unpack_VTD_GCMD_REG(&gcmd, 0);
		gcmd.srtp=1;
		_vtd_reg_write(drhd, VTD_GCMD_REG_OFF, pack_VTD_GCMD_REG(&gcmd));

		//ensure the RET address was latched by the h/w
		//gsts.value = _vtd_reg_read(drhd, VTD_GSTS_REG_OFF);
        unpack_VTD_GSTS_REG(&gsts, _vtd_reg_read(drhd, VTD_GSTS_REG_OFF));

		if(!gsts.rtps){
			//_XDPRINTF_("Error	Failed to latch RTADDR\n");
			return false;
		}
	}
	//_XDPRINTF_("DRHD RET initialized.\n");

	return true;
}


//enable VT-d translation
void xmhfhw_platform_x86pc_vtd_drhd_enable_translation(VTD_DRHD *drhd){
	VTD_GCMD_REG gcmd;
	VTD_GSTS_REG gsts;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if (drhd == NULL)
		return;


	//turn on translation
	//_XDPRINTF_("\nEnabling VT-d translation...");
	{
		unpack_VTD_GCMD_REG(&gcmd, 0);
		gcmd.te=1;
		#ifdef __XMHF_VERIFICATION_DRIVEASSERTS__
		assert(gcmd.te == 1);
		#endif

		_vtd_reg_write(drhd, VTD_GCMD_REG_OFF, pack_VTD_GCMD_REG(&gcmd));

		//wait for translation enabled status to go green...
		//gsts.value = _vtd_reg_read(drhd, VTD_GSTS_REG_OFF);
        unpack_VTD_GSTS_REG(&gsts, _vtd_reg_read(drhd, VTD_GSTS_REG_OFF));

		while(!gsts.tes){
			//gsts.value = _vtd_reg_read(drhd, VTD_GSTS_REG_OFF);
            unpack_VTD_GSTS_REG(&gsts, _vtd_reg_read(drhd, VTD_GSTS_REG_OFF));
		}
	}
	//_XDPRINTF_("\nVT-d translation enabled.");

	return;
}

//disable VT-d translation
void xmhfhw_platform_x86pc_vtd_drhd_disable_translation(VTD_DRHD *drhd){
	VTD_GCMD_REG gcmd;
	VTD_GSTS_REG gsts;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if ( drhd == NULL)
		return;

	//disable translation
	//_XDPRINTF_("\nDisabling VT-d translation...");
	{
		unpack_VTD_GCMD_REG(&gcmd, 0);
		gcmd.te=0;

		_vtd_reg_write(drhd, VTD_GCMD_REG_OFF, pack_VTD_GCMD_REG(&gcmd));

		//wait for translation enabled status to go red...
		//gsts.value = _vtd_reg_read(drhd, VTD_GSTS_REG_OFF);
        unpack_VTD_GSTS_REG(&gsts, _vtd_reg_read(drhd, VTD_GSTS_REG_OFF));

		while(gsts.tes){
			//gsts.value = _vtd_reg_read(drhd, VTD_GSTS_REG_OFF);
            unpack_VTD_GSTS_REG(&gsts, _vtd_reg_read(drhd, VTD_GSTS_REG_OFF));
		}
	}
	//_XDPRINTF_("\nVT-d translation disabled.");

	return;
}

//enable protected memory region (PMR)
void xmhfhw_platform_x86pc_vtd_drhd_enable_pmr(VTD_DRHD *drhd){
    VTD_PMEN_REG pmen;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if (drhd == NULL)
		return;

	//_XDPRINTF_("\nEnabling PMR...");
	{
		pmen.epm=1;	//enable PMR
		_vtd_reg_write(drhd, VTD_PMEN_REG_OFF, pack_VTD_PMEN_REG(&pmen));

		//wait for PMR enabled...
		do{
			//pmen.value = _vtd_reg_read(drhd, VTD_PMEN_REG_OFF);
			unpack_VTD_PMEN_REG(&pmen, _vtd_reg_read(drhd, VTD_PMEN_REG_OFF));
		}while(!pmen.prs);
	}
	//_XDPRINTF_("\nDRHD PMR enabled.");

}

//disable protected memory region (PMR)
void xmhfhw_platform_x86pc_vtd_drhd_disable_pmr(VTD_DRHD *drhd){
    VTD_PMEN_REG pmen;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if(drhd == NULL)
		return;

	//_XDPRINTF_("\nDisabling PMR...");
	{
		pmen.epm=0;	//disable PMR
		_vtd_reg_write(drhd, VTD_PMEN_REG_OFF, pack_VTD_PMEN_REG(&pmen));

		//wait for PMR disabled...
		do{
			//pmen.value = _vtd_reg_read(drhd, VTD_PMEN_REG_OFF);
			unpack_VTD_PMEN_REG(&pmen, _vtd_reg_read(drhd, VTD_PMEN_REG_OFF));
		}while(pmen.prs);
	}
	//_XDPRINTF_("\nDRHD PMR disabled.");

}

//set DRHD PLMBASE and PLMLIMIT PMRs
void xmhfhw_platform_x86pc_vtd_drhd_set_plm_base_and_limit(VTD_DRHD *drhd, u32 base, u32 limit){
	VTD_PLMBASE_REG plmbase;
	VTD_PLMLIMIT_REG plmlimit;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if(drhd == NULL)
		return;

	//set PLMBASE register
	plmbase.value = base;
	_vtd_reg_write(drhd, VTD_PLMBASE_REG_OFF, plmbase.value);

	//set PLMLIMIT register
	plmlimit.value = limit;
	_vtd_reg_write(drhd, VTD_PLMLIMIT_REG_OFF, plmlimit.value);
}


//set DRHD PHMBASE and PHMLIMIT PMRs
void xmhfhw_platform_x86pc_vtd_drhd_set_phm_base_and_limit(VTD_DRHD *drhd, u64 base, u64 limit){
	VTD_PHMBASE_REG phmbase;
	VTD_PHMLIMIT_REG phmlimit;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	//sanity check
	if (drhd == NULL)
		return;

	//set PHMBASE register
	phmbase.value = base;
	_vtd_reg_write(drhd, VTD_PHMBASE_REG_OFF, phmbase.value);

	//set PLMLIMIT register
	phmlimit.value = limit;
	_vtd_reg_write(drhd, VTD_PHMLIMIT_REG_OFF, phmlimit.value);
}

//read VT-d register
u64 xmhfhw_platform_x86pc_vtd_drhd_reg_read(VTD_DRHD *drhd, u32 reg){
    u64 __regval=0;
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	return _vtd_reg_read(drhd, reg);
}

//write VT-d register
void xmhfhw_platform_x86pc_vtd_drhd_reg_write(VTD_DRHD *drhd, u32 reg, u64 value){
	//VTD_DRHD *drhd = _vtd_get_drhd_struct(drhd_handle);

	_vtd_reg_write(drhd, reg, value);
}

