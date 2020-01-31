/*
 *	H_Branches.h
 *
 *
 */
#pragma once




namespace ARM
{



	inline static ptrdiff_t Literal(unat FnAddr)
	{
		u8* pc_addr = (u8*)EMIT_GET_PTR();
		return (ptrdiff_t)((ptrdiff_t)FnAddr - ((ptrdiff_t)pc_addr+8));
		//return -(ptrdiff_t)((pc_addr+8)-(ptrdiff_t)FnAddr);
	}

	EAPI CALL(unat FnAddr, ConditionCode CC=AL)
	{
        bool isThumb = FnAddr & 1;
        FnAddr &= ~1;
        ptrdiff_t lit = Literal(FnAddr);

		if(0==lit) {
			printf("Error, Compiler caught NULL literal, CALL(%08zX)\n", FnAddr);
			verify(false);
			return;
		}
		if( (lit<-33554432) || (lit>33554428) )     // ..28 for BL ..30 for BLX
		{
			printf("Warning, CALL(%08zX) is out of range for literal(%08zX)\n", FnAddr, lit);
			// verify(false);

			MOV32(IP, FnAddr, CC);
			BLX(IP, CC);
			return;
		}

        if (isThumb) {
            verify (CC==CC_EQ);
            BLX(lit, isThumb);
        } else {
            BL(lit,CC);
        }
	}



	EAPI JUMP(unat FnAddr, ConditionCode CC=AL)
	{
        bool isThumb = FnAddr & 1;
        FnAddr &= ~1;
        
        verify(!isThumb);
        ptrdiff_t lit = Literal(FnAddr);

		/*if(0==lit) {
			printf("Error, Compiler caught NULL literal, JUMP(%08X)\n", FnAddr);
			verify(false);
			return;
		}*/
		if( (lit<-33554432) || (lit>33554428) )     // ..28 for BL ..30 for BLX
		{
			printf("Warning, %zX is out of range for imm jump! \n", FnAddr);
			//verify(false);

			MOV32(IP, FnAddr, CC);
			BX(IP, CC);
			return;
		}
        B(lit,CC);     // Note, wont work for THUMB*,  have to use bx which is reg only !
	}



}




