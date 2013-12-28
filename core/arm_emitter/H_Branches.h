/*
 *	H_Branches.h
 *
 *
 */
#pragma once




namespace ARM
{



	inline static snat Literal(unat FnAddr)
	{
		u8* pc_addr = (u8*)EMIT_GET_PTR();
		return (snat)((snat)FnAddr - ((snat)pc_addr+8));
		//return -(snat)((pc_addr+8)-(snat)FnAddr);
	}

	EAPI CALL(unat FnAddr, ConditionCode CC=AL)
	{
		snat lit = Literal(FnAddr);

		if(0==lit) {
			printf("Error, Compiler caught NULL literal, CALL(%08X)\n", FnAddr);
			verify(false);
			return;
		}
		if( (lit<-33554432) || (lit>33554428) )     // ..28 for BL ..30 for BLX
		{
			printf("Warning, CALL(%08X) is out of range for literal(%08X)\n", FnAddr, lit);
			// verify(false);

			MOV32(IP, FnAddr, CC);
			BLX(IP, CC);
			return;
		}

		BL(lit,CC);
	}



	EAPI JUMP(unat FnAddr, ConditionCode CC=AL)
	{
		snat lit = Literal(FnAddr);

		/*if(0==lit) {
			printf("Error, Compiler caught NULL literal, JUMP(%08X)\n", FnAddr);
			verify(false);
			return;
		}*/
		if( (lit<-33554432) || (lit>33554428) )     // ..28 for BL ..30 for BLX
		{
			printf("Warning, %X is out of range for imm jump! \n", FnAddr);
			//verify(false);

			MOV32(IP, FnAddr, CC);
			BX(IP, CC);
			return;
		}

		B(lit,CC);     // Note, wont work for THUMB*,  have to use bx which is reg only !
	}



}




