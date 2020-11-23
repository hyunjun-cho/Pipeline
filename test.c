
/*                                                             */
/*   MIPS-32 Instruction Level Simulator                       */
/*                                                             */
/*   SCE212 Ajou University                                    */
/*   run.c                                                     */
/*   Adapted from CS311@KAIST                                  */
/*                                                             */
/***************************************************************/

#include <stdio.h>
#include <unistd.h>
#include "util.h"
#include "run.h"

/***************************************************************/
/*                                                             */
/* Procedure: get_inst_info                                    */
/*                                                             */
/* Purpose: Read insturction information                       */
/*                                                             */
/***************************************************************/
instruction* get_inst_info(uint32_t pc) {
    return &INST_INFO[(pc - MEM_TEXT_START) >> 2];
}

/***************************************************************/
/*                                                             */
/* Procedure: process_instruction                              */
/*                                                             */
/* Purpose: Process one instrction                             */
/*                                                             */
/***************************************************************/

static instruction* inst;
static int count = 0;
static int stall_var = FALSE;
void IF_Stage() {
	
	
	if(CURRENT_STATE.PIPE_STALL[IF_STAGE]) {	
		CURRENT_STATE.PIPE_STALL[IF_STAGE] = 0;
		CURRENT_STATE.PIPE[IF_STAGE] = 0;
		return;
	}	
	
	inst = get_inst_info(CURRENT_STATE.PC);
	if(!JUMP_BIT)
		CURRENT_STATE.PC += 4;

}

void ID_Stage() {
	
	if(CURRENT_STATE.PIPE[ID_STAGE] == 0)
		return;

	if(CURRENT_STATE.PIPE_STALL[ID_STAGE]) {
		CURRENT_STATE.PIPE_STALL[ID_STAGE] = 0;
		CURRENT_STATE.PIPE[ID_STAGE] = 0;
		return;
		
	}
	if(CURRENT_STATE.PC != MEM_TEXT_START)
		inst = get_inst_info(CURRENT_STATE.PIPE[ID_STAGE]);


	if(CURRENT_STATE.EX_MEM_W_VALUE) {	
		switch(OPCODE(inst)) {
			case 0x09:
			case 0x23:	
				if(RS(inst) == CURRENT_STATE.EX_MEM_ALU_OUT) {
					CURRENT_STATE.EX_MEM_W_VALUE = FALSE;
					CURRENT_STATE.EX_MEM_ALU_OUT = FALSE;
					CURRENT_STATE.PC -= 4;
					stall_var = TRUE;
					return;
				}
				CURRENT_STATE.EX_MEM_W_VALUE = FALSE;
				CURRENT_STATE.EX_MEM_ALU_OUT = FALSE;
				break;
			case 0x00:
				if(inst->func_code != 0x08) {
					if(RS(inst) == CURRENT_STATE.EX_MEM_ALU_OUT || RT(inst) == CURRENT_STATE.EX_MEM_ALU_OUT) {
						CURRENT_STATE.EX_MEM_W_VALUE = FALSE;
						CURRENT_STATE.EX_MEM_ALU_OUT = FALSE;
						CURRENT_STATE.PC -= 4;
						stall_var = TRUE;
						return;
					}
				}
				CURRENT_STATE.EX_MEM_W_VALUE = FALSE;
				CURRENT_STATE.EX_MEM_ALU_OUT = FALSE;
				break;
		}
	}

	switch(OPCODE(inst)) {
		case 0x04:
			if(CURRENT_STATE.REGS[RS(inst)] == CURRENT_STATE.REGS[RT(inst)]) {
				CURRENT_STATE.BRANCH_PC = CURRENT_STATE.PIPE[EX_STAGE] + (IMM(inst) * 4) + 8;
//				BR_BIT = FALSE;
			}
			break;
		case 0x05:
			if(CURRENT_STATE.REGS[RS(inst)] != CURRENT_STATE.REGS[RT(inst)]) {
				CURRENT_STATE.BRANCH_PC = CURRENT_STATE.PIPE[EX_STAGE] + (IMM(inst) * 4) + 8;
//				BR_BIT = FALSE;
			}
			break;
		case 0x00:
			if(inst-> func_code == 0x08) {
				CURRENT_STATE.ID_EX_REG1 = CURRENT_STATE.REGS[RS(inst)];
				CURRENT_STATE.JUMP_PC = CURRENT_STATE.REGS[RS(inst)];
				JUMP_BIT = TRUE;
			}
			else{
				CURRENT_STATE.ID_EX_REG1 = CURRENT_STATE.REGS[RS(inst)];
				CURRENT_STATE.ID_EX_REG2 = CURRENT_STATE.REGS[RT(inst)];
			}
			break;
		case 0x02:
			CURRENT_STATE.ID_EX_DEST = TARGET(inst);
			CURRENT_STATE.JUMP_PC = (CURRENT_STATE.PIPE[EX_STAGE] & 0xf000) | (TARGET(inst) << 2);
			JUMP_BIT = TRUE;
			break;
		case 0x03:
			CURRENT_STATE.ID_EX_DEST = TARGET(inst);
			CURRENT_STATE.REGS[31] = CURRENT_STATE.PC + 4;
			CURRENT_STATE.JUMP_PC = (CURRENT_STATE.PIPE[EX_STAGE] & 0xf000) | (TARGET(inst) << 2);
			JUMP_BIT = TRUE;
			break;
		case 0x23:
			CURRENT_STATE.REGS_LOCK[RT(inst)] = TRUE;
			CURRENT_STATE.EX_MEM_W_VALUE = TRUE;
			CURRENT_STATE.EX_MEM_ALU_OUT = RT(inst);
			CURRENT_STATE.EX_MEM_FORWARD_VALUE = RT(inst);
			FORWARDING_BIT = FALSE;
			break;
	}
}
void EX_Stage() {
	instruction* hazard_inst = get_inst_info(CURRENT_STATE.PIPE[MEM_STAGE]);

	if(CURRENT_STATE.PIPE[EX_STAGE] == 0)
		return;

	if(CURRENT_STATE.PIPE_STALL[EX_STAGE]){
		CURRENT_STATE.PIPE_STALL[EX_STAGE] = 0;
		CURRENT_STATE.PIPE[EX_STAGE] = 0;
		return;
	}
	
	inst = get_inst_info(CURRENT_STATE.PIPE[EX_STAGE]);

	switch(OPCODE(inst)) {
		case 0x02:
		case 0x03:	
			CURRENT_STATE.PIPE[ID_STAGE] = FALSE;
			break;
		case 0x04:			//type beq
			if(CURRENT_STATE.REGS[RS(inst)] == CURRENT_STATE.REGS[RT(inst)]) {
				BR_BIT = FALSE;
			}
			break;
		case 0x05:			//type bne
			if(CURRENT_STATE.REGS[RS(inst)] != CURRENT_STATE.REGS[RT(inst)]) {
				BR_BIT = FALSE;
			}
			break;
		case 0x09:
			CURRENT_STATE.MEM_WB_ALU_OUT = SIGN_EX(IMM(inst)) + CURRENT_STATE.REGS[RS(inst)];
			break;
		case 0x0c:
			CURRENT_STATE.REGS[RT(inst)] = CURRENT_STATE.REGS[RS(inst)] & (IMM(inst));
			break;
		case 0x00:
			if(inst->func_code == 0x8){
				CURRENT_STATE.PIPE[ID_STAGE] = FALSE;
			}
			else if(inst->func_code == 0x23 || inst->func_code == 0x22) {
				CURRENT_STATE.REGS[RD(inst)] = CURRENT_STATE.REGS[RS(inst)] - CURRENT_STATE.REGS[RT(inst)];
			}
			else if(inst->func_code == 0x00) {
				CURRENT_STATE.REGS[RD(inst)] = CURRENT_STATE.REGS[RT(inst)] << SHAMT(inst);
			}
			else if(inst->func_code == 0x02) {
				CURRENT_STATE.REGS[RD(inst)] = CURRENT_STATE.REGS[RT(inst)] >> SHAMT(inst);
			}
			else if(inst->func_code == 0x2b || inst->func_code == 0x2a) {
				CURRENT_STATE.REGS[RD(inst)] = (CURRENT_STATE.REGS[RS(inst)] < CURRENT_STATE.REGS[RT(inst)]) ? 1 : 0;
			}	
			else if(inst->func_code == 0x25) {
				CURRENT_STATE.REGS[RD(inst)] = CURRENT_STATE.REGS[RS(inst)] | CURRENT_STATE.REGS[RT(inst)];
			}
			else if(inst->func_code == 0x27) {
				CURRENT_STATE.REGS[RD(inst)] += ~(CURRENT_STATE.REGS[RS(inst)] | CURRENT_STATE.REGS[RT(inst)]);
			}
			else if(inst->func_code == 0x21 || inst->func_code == 0x20) {
				CURRENT_STATE.REGS[RD(inst)] = CURRENT_STATE.REGS[RS(inst)] + CURRENT_STATE.REGS[RT(inst)];
			}
			else if(inst->func_code == 0x24) {
				CURRENT_STATE.REGS[RD(inst)] = CURRENT_STATE.REGS[RS(inst)] & CURRENT_STATE.REGS[RT(inst)];
			}
			break;
		case 0x0b:
			CURRENT_STATE.REGS[RT(inst)] = (CURRENT_STATE.REGS[RS(inst)] < SIGN_EX(IMM(inst))) ? 1 : 0;
			break;
		case 0x23:
			CURRENT_STATE.REGS[RT(inst)] = mem_read_32(CURRENT_STATE.REGS[RS(inst)] + SIGN_EX(IMM(inst)));
			CURRENT_STATE.EX_MEM_W_VALUE = TRUE;
			CURRENT_STATE.EX_MEM_ALU_OUT = RT(inst);
			FORWARDING_BIT = FALSE;
			break;
		case 0x2b:
			mem_write_32(CURRENT_STATE.REGS[RS(inst)] + SIGN_EX(IMM(inst)), CURRENT_STATE.REGS[RT(inst)]);	
			break;
		case 0xd:
			if(RT(inst) != RS(inst))
				CURRENT_STATE.REGS[RT(inst)] += CURRENT_STATE.REGS[RS(inst)] | (IMM(inst));
			else
				CURRENT_STATE.REGS[RT(inst)] += IMM(inst);
			break;
		case 0xf:
			CURRENT_STATE.REGS[RT(inst)] = ((IMM(inst)) << 16);
			break;
			
	}

}

void MEM_Stage() {
	if(CURRENT_STATE.PIPE_STALL[MEM_STAGE] || !CURRENT_STATE.PIPE[MEM_STAGE])
		return;
	inst = get_inst_info(CURRENT_STATE.PIPE[MEM_STAGE]);

	switch(inst->opcode) {
		case 0x09:
			CURRENT_STATE.REGS[RT(inst)] = CURRENT_STATE.MEM_WB_ALU_OUT;
			break;
	}
	
	
}

void WB_Stage() {
	if(CURRENT_STATE.PIPE_STALL[WB_STAGE] || !CURRENT_STATE.PIPE[WB_STAGE])
		return;

	inst = get_inst_info(CURRENT_STATE.PIPE[WB_STAGE]);
	/*
	switch(inst->opcode) {
		case 0x09:
			CURRENT_STATE.REGS[RT(inst)] = CURRENT_STATE.MEM_WB_ALU_OUT;
			break;
	}
*/

	INSTRUCTION_COUNT++;

}
void process_instruction() {
	/** Your implementation here */

	inst = (instruction*)malloc(sizeof(instruction));
	int status = TRUE;
	int temp_pc = 0;

	if(JUMP_BIT && !BR_BIT)
		JUMP_BIT = FALSE;

	if(JUMP_BIT) {
		for(int i = PIPE_STAGE - 1; i > 0; i--) {
			CURRENT_STATE.PIPE[i] = CURRENT_STATE.PIPE[i-1];
		}
		CURRENT_STATE.PC = CURRENT_STATE.JUMP_PC;
		CURRENT_STATE.PIPE[IF_STAGE] = CURRENT_STATE.PC;
		JUMP_BIT = FALSE;
		goto PIPE;
	}
	if(!BR_BIT) {
		for(int i = PIPE_STAGE - 1; i > 2; i--) {
			CURRENT_STATE.PIPE[i] = CURRENT_STATE.PIPE[i-1];
		}
		CURRENT_STATE.PIPE_STALL[IF_STAGE] = TRUE;
		CURRENT_STATE.PIPE_STALL[ID_STAGE] = TRUE;
		CURRENT_STATE.PIPE_STALL[EX_STAGE] = TRUE;
		BR_BIT = TRUE;
		CURRENT_STATE.PC = CURRENT_STATE.BRANCH_PC;
		goto PIPE;
	}
	if(stall_var) {
		for(int i = PIPE_STAGE - 1; i > 2; i--)
		CURRENT_STATE.PIPE[i] = CURRENT_STATE.PIPE[i-1];
		CURRENT_STATE.PIPE[EX_STAGE] = 0;
		stall_var--;
		goto PIPE;
	}
	
	for(int i = PIPE_STAGE - 1; i > 0; i--) {
		CURRENT_STATE.PIPE[i] = CURRENT_STATE.PIPE[i-1];
	}
	CURRENT_STATE.PIPE[IF_STAGE] = CURRENT_STATE.PC;
		
	if (CURRENT_STATE.PC < MEM_REGIONS[0].start || CURRENT_STATE.PC >= (MEM_REGIONS[0].start + NUM_INST * 4)) {
		FETCH_BIT--;
		temp_pc = CURRENT_STATE.PC;
		CURRENT_STATE.PC = 0;
		status = FALSE;	
	}
	CURRENT_STATE.PIPE[IF_STAGE] = CURRENT_STATE.PC;	

PIPE:

	WB_Stage();
	MEM_Stage();
	EX_Stage();
	ID_Stage();
	IF_Stage();


	if(!status)	
		CURRENT_STATE.PC = temp_pc;

	if(FETCH_BIT == -3) {
		RUN_BIT = FALSE;
	}

}
