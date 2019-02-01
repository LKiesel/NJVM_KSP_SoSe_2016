#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "njvm_makros.h"
#include "njvm_opcode_def.h"
#include "njvm_util.h"
#include "bigint.h"
#include "njvm_exits.h"

typedef struct
{
	int isObjRef; // 1 = Obj; 0 != Obj;
	union
	{
		ObjRef objRef;
		int metaNr;
	}u;
}StackSlot;

int goPurging = 0;

int* programm_memory;
int instructionCount;
int pc = 0;

unsigned char* heap;
unsigned char* heapC;
unsigned char* heapStart;
unsigned char* heapEnd;
int isFlipped = 0;
int heapSize = -1;

ObjRef* global_memory;
int globals;

StackSlot* stack;
int stackSize = -1;
int stackC = 0;
int framePointer = 0;

ObjRef returnValue;

int returnAdress;

int instruction;

char* opToShow;

int gcNude = 0;

int freeSpace;
int copiedObjects = 0;
int copiedObjectSize = 0;
int allocatedObjects = 0;
int allocatedObjectSize = 0;

ObjRef relocate(ObjRef);
ObjRef copyObjToFreeMem(ObjRef);

int main(int argc, char* argv[])
{
	for (int i = 1; i < argc; i++){
		if(strcmp(argv[i], "--version") == 0){
			printf("Ninja Virtual Machine Version %i (compiled %s %s)\n",VERSION, __DATE__, __TIME__);
			return 0;
		}else if(strcmp(argv[i], "--help") == 0){
			showHelp();
			return 0;
		}else if(strcmp(argv[i], "--gcpurge") == 0){
			goPurging = 1;
		}else if(strcmp(argv[i], "--gcstats") == 0){
			gcNude = 1;
		}else if(strcmp(argv[i], "--stack") == 0){
			int size = atoi(argv[i+1]);
			i = i + 1;
			allocStack(size);
		}else if(strcmp(argv[i], "--heap") == 0){
			int size = atoi(argv[i+1]);
			printf("%d\n", size);
			i = i + 1;
			allocHeap(size);
		}else if (strcmp(argv[i], "--debug") == 0){
			if(heapSize == -1)allocHeap(8192);
			if(stackSize == -1)allocStack(64);
			openFiles(argv[i + 1]);
			startDebug();
			printf("Ninja Virtual Machine stopped\n");
			return 0;
		}else{
			if(heapSize == -1)allocHeap(8192);
			if(stackSize == -1)allocStack(64);
			openFiles(argv[i]);
			startProg();
			printf("Ninja Virtual Machine stopped\n");
			return 0;
		}
	}
	printf("No Arguments: --help for a list of Instructions\n");
	return 0;
}
void allocStack(int size)
{
	stackSize = (size * 1024);
	stack = malloc(stackSize);
}
void allocHeap(int size)
{
	heapSize = (size * 1024);
	heap = malloc(heapSize);
	heapC = heap;
	heapStart = heap;
	heapEnd = &heap[heapSize/2];
}
ObjRef alloc(int size)
{
	if ((heapC + size) >= heapEnd)	{
		freeSpace = heapSize/2;
		copiedObjects = 0;
		copiedObjectSize = 0;
		garbageColl();
		allocatedObjects = 0;
		allocatedObjectSize = 0;
		if (goPurging) purge();
		if ((heapC + size) >= heapEnd)
		{
			printf("%s\n", "Out of Memory");
			exit(OUTOFMEMORY);
		}else{
			allocatedObjects = allocatedObjects + 1;
			allocatedObjectSize = allocatedObjectSize + size;
			freeSpace = freeSpace - size;
			ObjRef objRef = (ObjRef)heapC;
			heapC = heapC + size;
			return objRef;
		}
	}else{
		allocatedObjects = allocatedObjects + 1;
		allocatedObjectSize = allocatedObjectSize + size;
		freeSpace = freeSpace - size;
		ObjRef objRef = (ObjRef)heapC;
		heapC = heapC + size;
		return objRef;
	}
	return NULL;
}

ObjRef newPrimObject(int dataSize) 
{
  ObjRef objRef;

  objRef = alloc(sizeof(unsigned int) + dataSize * sizeof(unsigned char));
  if (objRef == NULL) {
    fatalError("newPrimObject() got no memory");
  }
  objRef->size = dataSize;
  return objRef;
}

ObjRef popObj(void)
{
	if(stackC >= 0){
		if (stack[stackC-1].isObjRef == 0)	{
			printf("%s\n", "cant pop Number");
			exit(CONSTIPATION);
		}
		else stackC = stackC - 1;
		return stack[stackC].u.objRef;
	}else{
		printf("%s%d%s\n","Error: at: ", pc , "Stack underflow");
		exit(UNDERFLOW);
	}
}

int popNr(void)
{
	if(stackC >= 0){
		if (stack[stackC-1].isObjRef == 1){
			printf("%s\n", "cant pop Object");
			exit(CONSTIPATION_OBJ);
		}
		stackC = stackC - 1;
		return stack[stackC].u.metaNr;
	}else{
		printf("%s%d%s\n","Error: at: ", pc , "Stack underflow");
		exit(UNDERFLOW);
	}
}

void pushObj(ObjRef ref)
{
	if(&stack[stackC + 1] < &stack[stackSize]){
		stack[stackC].isObjRef = 1;
		stack[stackC].u.objRef = ref;
		stackC = stackC + 1;
	}else{
		printf("%s\n","Stack overflow");
		exit(OVERFLOW);
	}
}

void pushNr(int value)
{
	if(&stack[stackC + 1] < &stack[stackSize]){
		stack[stackC].isObjRef = 0;
		stack[stackC].u.metaNr = value;
		stackC = stackC + 1;
	}
	else{
		printf("%s\n","Stack overflow");
		exit(OVERFLOW);
	}
}

void pushC(int value)
{
	bigFromInt(value);
	ObjRef ref = bip.res;
	pushObj(ref);
}

void jmp(int i)
{
	pc = i;
}

void add(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	bigAdd();
	//ObjRef c = bip.res;
	pushObj((ObjRef)bip.res);
}

void sub(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	bigSub();
	//ObjRef c = bip.res;
	pushObj((ObjRef)bip.res);
}

void mul(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	bigMul();
	//ObjRef c = bip.res;
	pushObj((ObjRef)bip.res);
}

void divide(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	bigDiv();
	//ObjRef c = bip.res;
	pushObj((ObjRef)bip.res);
}

void mod(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	bigDiv();
	//ObjRef c = bip.rem;
	pushObj((ObjRef)bip.rem);
}

void rdint(void)
{
	bigRead(stdin);
	pushObj(bip.res);
}

void wrint(void)
{
	bip.op1 = popObj();
	bigPrint(stdout);
}

void rdchar(void)
{
	char c;
	scanf("%c", &c);
	bigFromInt(c);
	pushObj(bip.res);
}

void wrchar(void)
{
	bip.op1 = popObj();
	int c = bigToInt();
	printf("%c", (char)c);
}

void pushg(int i)
{		
	pushObj(global_memory[i]);
}

void popg(int i)
{
	global_memory[i] = popObj();
}

void asf(int j)
{
	pushNr(framePointer);
	framePointer = stackC;
	for (int i = 0; i < j; ++i)
	{
		pushObj(NULL);
	}
}

void rsf(void)
{
	stackC = framePointer;
	framePointer = popNr();
}

void pushl(int i)
{
	pushObj(stack[framePointer+i].u.objRef);
}
void popl(int i)
{
	stack[framePointer+i].u.objRef = popObj();
}
void eq(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	if(bigCmp() == 0){
		bigFromInt(1);
		pushObj(bip.res);
	} 
	else{
		bigFromInt(0);
		pushObj(bip.res);
	} 
}
void ne(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	if(bigCmp() == 0){
		bigFromInt(0);
		pushObj(bip.res);
	}
	else{
		bigFromInt(1);
		pushObj(bip.res);
	} 
}
void lt(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	if(bigCmp() < 0){
		bigFromInt(1);
		pushObj(bip.res);
	} 
	else{
		bigFromInt(0);
		pushObj(bip.res);
	} 
}
void le(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	if(bigCmp() <= 0){
		bigFromInt(1);
		pushObj(bip.res);
	}
	else{
		bigFromInt(0);
		pushObj(bip.res);
	} 
}

void gt(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	if(bigCmp() > 0){
		bigFromInt(1);
		pushObj(bip.res);
	} 
	else{
		bigFromInt(0);
		pushObj(bip.res);
	} 
}
void ge(void)
{
	bip.op2 = popObj();
	bip.op1 = popObj();
	if(bigCmp() >= 0){
		bigFromInt(1);
		pushObj(bip.res);
	} 
	else{
		bigFromInt(0);
		pushObj(bip.res);
	} 
}
void brf(int i)
{
	bip.op1 = popObj();
	int b = bigToInt(); 
	if(b == 0) jmp(i);
}
void brt(int i)
{
	bip.op1 = popObj();
	int b = bigToInt();
	if(b == 1) jmp(i);
}

void call(int i)
{
	pushNr(pc);
	jmp(i);
}

void ret()
{
	jmp(popNr());
}

void drop(int n)
{
	for(int i = 0; i <= n; i++){
		stack[stackC - i].u.objRef = NULL;
	}
	stackC = stackC - n;
}

void pushr(void)
{
	pushObj(returnValue);
}

void popr(void)
{
	returnValue = popObj();
}

void dup(void)
{
	ObjRef i = popObj();
	pushObj(i);
	pushObj(i);
}
void new(int value)
{
	ObjRef ref = alloc((value*sizeof(ObjRef))+sizeof(unsigned int));
	ref -> size = (value | MSB);
	if (value < 0)
	{
		printf("%s\n", "Size must be > 0");
		exit(MICRO_PP);
	}
	for (int i = 0; i < value; ++i)
	{
		GET_REFS(ref)[i] = NULL;
	}
	pushObj(ref);
}
void getF(int value)
{
	ObjRef obj = popObj();
	if (value >= GET_SIZE(obj))
	{
		printf("%s\n", "Record out of bounds");
		exit(OUT_OF_BOUNDS_REC);
	}
	pushObj(GET_REFS(obj)[value]);
}
void putF(int value)
{
	ObjRef field = popObj();
	ObjRef obj = popObj();
	if (value >= GET_SIZE(obj))
	{
		printf("%s\n", "Record out of bounds");
		exit(OUT_OF_BOUNDS_REC);
	}
	GET_REFS(obj)[value] = field;
}
void newA(void)
{
	bip.op1 = popObj();
	int size = bigToInt();
	ObjRef ref = alloc(size*sizeof(ObjRef)+sizeof(unsigned int));
	ref -> size = (size | MSB);
	if (size < 0)
	{
		printf("%s\n", "Size must be > 0");
		exit(MICRO_PP);
	}
	for (int i = 0; i < size; ++i)
	{
		GET_REFS(ref)[i] = NULL;
	}
	pushObj(ref);
}
void getFA(void)
{
	bip.op1 = popObj();
	int index = bigToInt();
	ObjRef array = popObj();
	if (index >= GET_SIZE(array))
	{
		printf("%s%d\n", "get: Array out of bounds line: ", pc);
		exit(OUT_OF_BOUNDS_ARRAY);
	}
	else pushObj(GET_REFS(array)[index]);
}
void putFA(void)
{
	ObjRef field = popObj();
	bip.op1 = popObj();
	int index = bigToInt();
	ObjRef array = popObj();
	if (index >= GET_SIZE(array))
	{
		printf("%s%d\n", "set: Array out of bounds line: ", pc);
		exit(OUT_OF_BOUNDS_ARRAY);
	}
	else GET_REFS(array)[index] = field;
}
void getSZ(void)
{
	ObjRef obj = popObj();
	if (IS_PRIM(obj))
	{
		bigFromInt(-1);
		pushObj(bip.res);
	}else{
		int size = GET_SIZE(obj);
		bigFromInt(size);
		pushObj(bip.res);
	}
}
void pushN(void)
{
	pushObj(NULL);
}
void refEQ(void)
{
	ObjRef ref1 = popObj();
	ObjRef ref2 = popObj();
	if(ref1 == ref2){
		bigFromInt(1);
		pushObj(bip.res);
	}else{
		bigFromInt(0);
		pushObj(bip.res);
	}
}
void refNE(void)
{
	ObjRef ref1 = popObj();
	ObjRef ref2 = popObj();
	if(ref1 == ref2){
		bigFromInt(0);
		pushObj(bip.res);
	}else{
		bigFromInt(1);
		pushObj(bip.res);
	}
}
void startProg(void)
{
	printf("Ninja Virtual Machine started\n");
	while(OPCODE(programm_memory[pc]) != HALT){
		instruction = programm_memory[pc];
		pc = pc + 1;
		switchTheCases(instruction);
		if (OPCODE(programm_memory[pc]) == HALT)
		{
			if (gcNude == 1)
			{
				freeSpace = heapSize/2;
				copiedObjects = 0;
				copiedObjectSize = 0;
				garbageColl();
			}
		}
	}
}
void startDebug(void)
{
	printf("Ninja Virtual Machine started\n");
	char debugCommand[12];
	while(OPCODE(programm_memory[pc]) != HALT){
		printf("!");
		showMeWhatYouGot();
		printf("Debug with followig commands: list, Breakpoint, step, exit\n");
		scanf("%11s", debugCommand);
		if(strcmp("exit", debugCommand) == 0){
			exit(ZERO_PROBLEMOS);
		}
		else if(strcmp("list", debugCommand) == 0){
			printf("list: Programm Memory, Global Variables, Stack, Programmcounter, Object\n");
			scanf("%11s", debugCommand);
			if(strcmp("programm", debugCommand) == 0){
				if(instructionCount == 0) printf("No programm memory\n");
				for(int i; i < instructionCount; i++){
					showOpcode(OPCODE(programm_memory[i]));
					printf("%d%s%s%s%d\n",i, ": ", opToShow, ": ",SIGN_EXTEND(IMMEDIATE(programm_memory[i])));
				}
			}
			else if(strcmp("obj", debugCommand) == 0){
				printf("%s\n", "Enter Adress");
				void* pointer;
				scanf("%p", (void**)&pointer);
				ObjRef objRef = (ObjRef)pointer;
				if(IS_PRIM(objRef)){
					bip.op1 = objRef;
					printf("%s%d\n","Contains", bigToInt());
				} else{
					for (int i = 0; i < GET_SIZE(objRef); ++i)
					{
						printf("%p\n", (void *)GET_REFS(objRef)[i]);
					}
				}
			}
			else if(strcmp("globals", debugCommand) == 0){
				if(globals == 0) printf("No gloabal variables\n");
				char* type;
				for(int i = 0; i < globals; i++){
					if(IS_PRIM(global_memory[i])) type = "Prim Object";
					else type = "Compound Object";
					printf("%d%s%s%s\n", i, ":", "\t", type);
				}
				printf("%s\n", "Inspect global data or step");
				scanf("%11s", debugCommand);
				if (strcmp("inspect", debugCommand) == 0)
				{
					printf("%s\n", "Slot Number: ");
					int cmd;
					scanf("%d", &cmd);
					if (IS_PRIM(global_memory[cmd]))
					{
						bip.op1 = global_memory[cmd];
						int tmp = bigToInt();
						printf("%s%d%s%p\n", "Prim Object: ", tmp, "Adress: ", (void *)global_memory[cmd]);
					}else{
						int size = GET_SIZE(global_memory[cmd]);
						for (int i = 0; i < size; ++i)
						{
							printf("%p\n", (void *)GET_REFS(global_memory[cmd])[i]);
						}
					}
				}else if(strcmp("step", debugCommand) == 0){
					
					instruction = programm_memory[pc];
					pc = pc + 1;
				}
			}
			else if (strcmp("stack", debugCommand) == 0){
				if(stackC == 0) printf("Empty stack\n");
				else{
					char* stackType;
					for (int i = 0; i < stackC; i++){
						if (stack[i].isObjRef == 0) stackType = "Meta Number";
						else{
							if (stack[i].u.objRef == NULL) stackType = "NULL";
							else if (IS_PRIM(stack[i].u.objRef)) stackType = "Prim Object";
							else stackType = "Compound Object";
						}
						printf("%d%s%s%s\n", i,":", "\t", stackType);
					}
				}
				printf("FramePointer: %d\n", framePointer);
				printf("%s\n", "Inspect StackSlots or step");
				scanf("%11s", debugCommand);
				if (strcmp("inspect", debugCommand) == 0)
				{
					printf("%s\n", "Slot Number: ");
					int cmd;
					scanf("%d", &cmd);
					if(stack[cmd].isObjRef == 0){
						printf("%s%d\n", "Number is: ", stack[cmd].u.metaNr);
					}else{
						if (IS_PRIM(stack[cmd].u.objRef))
						{
							bip.op1 = stack[cmd].u.objRef;
							int tmp = bigToInt();
							printf("%s%d%s%p\n", "Prim Object: ", tmp, "Adress: ", (void *)stack[cmd].u.objRef);
						}else{
							int size = GET_SIZE(stack[cmd].u.objRef);
							for (int i = 0; i < size; ++i)
							{
								printf("%p\n", (void *)GET_REFS(stack[cmd].u.objRef)[i]);
							}
						}
					}
				}else if(strcmp("step", debugCommand) == 0){
					instruction = programm_memory[pc];
					pc = pc + 1;
					switchTheCases(instruction);
				}
			}
			else if(strcmp("pc", debugCommand) == 0){
				printf("PC: %d\n", pc);
			}
		}
		else if(strcmp("BreakP", debugCommand) == 0){
			int breakpoint;
			printf("Set your Breakpoint\n");
			scanf("%d", &breakpoint);
			for (int i = 0; i < breakpoint; i++){
				instruction = programm_memory[pc];
				pc = pc + 1;
				switchTheCases(instruction);
			}
		}
		else if(strcmp("step", debugCommand) == 0){
			instruction = programm_memory[pc];
			pc = pc + 1;
			switchTheCases(instruction);
		}
	}
}
void showHelp(void)
{
	printf("--version\tshow Version and exit\n");
	printf("--help\t\tshow this help and exit\n");
	printf("--debug\t\tstart your programm in the debugging mode\n");
}

void openFiles(char* path)
{
	FILE* file = fopen(path, "r");
	if (!(file != 0)){
		printf("%s\n", "No File selected");
		exit(NO_FILE_SELECTED);
	}
	char format[5];
	format[4] = '\0';
	int version_file;
	
	
	fread(format, sizeof(char), 4, file);
	if(strcmp(format, "NJBF")){
		printf("%s\n", "Invalid Format");
		exit(INV_FORMAT);
	}
	fread(&version_file, sizeof(int), 1, file);
	if (version_file > VERSION){
		printf("Version conflict: njvm: %d programm: %d\n", VERSION, version_file);
		exit(VERSION_CONF);
	}
	fread(&instructionCount, sizeof(int), 1, file);
	programm_memory = malloc(sizeof(int)*instructionCount);
	fread(&globals, sizeof(int),1, file);
	global_memory = malloc(sizeof(ObjRef)*globals);
	fread(programm_memory, sizeof(int), instructionCount, file);
	fclose(file);
}

void switchTheCases(int instruction)
{
	int opcode = OPCODE(instruction);
	int immidiate = SIGN_EXTEND(IMMEDIATE(instruction));
	switch (opcode){
		case PUSHC: pushC(immidiate); break;

		case ADD: add(); break;
		case SUB: sub(); break;
		case MUL: mul(); break;
		case DIV: divide(); break;
		case MOD: mod(); break;

		case RDINT: rdint(); break;
		case WRINT: wrint(); break;
		case WRCHAR: wrchar(); break;
		case RDCHAR: rdchar(); break;

		case PUSHG: pushg(immidiate); break; 
		case POPG: popg(immidiate); break;

		case ASF: asf(immidiate); break;
		case RSF: rsf(); break;

		case PUSHL: pushl(immidiate); break;
		case POPL: popl(immidiate); break;

		case EQ: eq(); break;
		case NE: ne(); break;
		case LE: le(); break;
		case LT: lt(); break;
		case GE: ge(); break;
		case GT: gt(); break;

		case JMP: jmp(immidiate); break;
		case BRF: brf(immidiate); break;
		case BRT: brt(immidiate); break;

		case CALL: call(immidiate); break;
		case RET: ret(); break;
		case DROP: drop(immidiate); break;
		case PUSHR: pushr(); break;
		case POPR:  popr(); break;

		case DUP: dup(); break;

		case NEW: new(immidiate);break;
		case GETF: getF(immidiate);break;
		case PUTF: putF(immidiate);break;

		case NEWA: newA();break;
		case GETFA: getFA();break;
		case PUTFA: putFA();break;

		case GETSZ: getSZ();break;

		case PUSHN: pushN();break;
		case REFEQ: refEQ();break;
		case REFNE: refNE();break;

		default: printf("Error no command selected\n"); exit(VERSION_CONF); break;
	}
}

void showOpcode(int opcode)
{
	switch (opcode){
		case PUSHC: opToShow = "PUSHC"; break;
		case ADD: opToShow = "ADD"; break;
		case SUB: opToShow = "SUB"; break;
		case MUL: opToShow = "MUL"; break;
		case DIV: opToShow = "DIV"; break;

		case RDINT: opToShow = "RDINT"; break;
		case WRINT: opToShow = "WRINT"; break;
		case WRCHAR: opToShow = "WRCHAR"; break;
		case RDCHAR: opToShow = "RDCHAR"; break;

		case PUSHG: opToShow = "PUSHG"; break;
		case POPG: opToShow = "POPG"; break;

		case ASF: opToShow = "ASF"; break;
		case RSF: opToShow = "RSF"; break;

		case PUSHL: opToShow = "PUSHL"; break;
		case POPL: opToShow = "POPL"; break;

		case EQ: opToShow = "EQ"; break;
		case NE: opToShow = "NE"; break;
		case LE: opToShow = "LE"; break;
		case LT: opToShow = "LT"; break;
		case GE: opToShow = "GE"; break;
		case GT: opToShow = "GT"; break;

		case JMP: opToShow = "JMP"; break;
		case BRF: opToShow = "BRF"; break;
		case BRT: opToShow = "BRT"; break;

		case CALL: opToShow = "CALL"; break;
		case RET: opToShow = "RET"; break;
		case DROP: opToShow = "DROP"; break;
		case PUSHR: opToShow = "PUSHR"; break;
		case POPR: opToShow = "POPR"; break;

		case DUP: opToShow = "DUP"; break;

		case NEW: opToShow = "NEW";break;
		case GETF: opToShow = "GETF";break;
		case PUTF: opToShow = "PUTF";break;

		case NEWA: opToShow = "NEWA";break;
		case GETFA: opToShow = "GETFA";break;
		case PUTFA: opToShow = "PUTFA";break;

		case GETSZ: opToShow = "GETSZ";break;

		case PUSHN: opToShow = "PUSHN";break;
		case REFEQ: opToShow = "REFEQ";break;
		case REFNE: opToShow = "REFNE";break;
	}
}

void showMeWhatYouGot(void)
{
	showOpcode(OPCODE(programm_memory[pc]));
	printf("%s%s%d\n", opToShow, ": ", SIGN_EXTEND(IMMEDIATE(programm_memory[pc])));
}

void fatalError(char *msg) 
{
  printf("Fatal error: %s\n", msg);
  exit(FATALITY);
}

void garbageColl(void)
{
	if(isFlipped == 0)
	{
		heapC = &heap[heapSize/2];
		heapStart = &heap[heapSize/2];
		heapEnd = &heap[heapSize];
		isFlipped = !isFlipped;
	}else if(isFlipped == 1)
	{
		heapC = &heap[0];
		heapStart = &heap[0];
		heapEnd = &heap[heapSize/2];
		isFlipped = !isFlipped;
	}
	for (int i = 0; i < stackC; i++)
	{
		if (stack[i].isObjRef == 1)
		{
			stack[i].u.objRef = relocate(stack[i].u.objRef);
		}
	}
	for (int i = 0; i < globals; i++)
	{
		global_memory[i] = relocate(global_memory[i]);
	}
	returnValue = relocate(returnValue);
	bip.op1 = relocate(bip.op1);
	bip.op2 = relocate(bip.op2);
	bip.res = relocate(bip.res);
	bip.rem = relocate(bip.rem);

	unsigned char* scan = heapStart;
	while(scan != heapC){
		if(!IS_PRIM((ObjRef)scan)){
			for (int i = 0; i < (GET_SIZE((ObjRef)scan)); i++)
			{
				GET_REFS((ObjRef)scan)[i] = relocate(GET_REFS((ObjRef)scan)[i]);
			}
			scan = scan + GET_SIZE((ObjRef)scan)*sizeof(ObjRef) + sizeof(unsigned int);
		}
		else scan = scan + GET_SIZE((ObjRef)scan) + sizeof(unsigned int);
	}
	if(gcNude == 1) gcGoneWild();
}

ObjRef relocate(ObjRef origin)
{
	ObjRef copy;
	if(origin == NULL){
		copy = NULL;
	}else if(HAS_BROKEN_HEART(origin)){
		copy = (ObjRef)(heapStart + FORWARD_POINTER(origin));
	}else{
		copy = copyObjToFreeMem(origin);
		origin -> size = ((origin -> size) & (MSB)) | (BROKEN_HEART) | (((unsigned char*)copy) - heapStart);
	}
	return copy;
}

ObjRef copyObjToFreeMem(ObjRef origin)
{
	ObjRef copy;
	int size;
	if (IS_PRIM(origin)){
		size = GET_SIZE(origin) + sizeof(unsigned int);
	}else{
		size = GET_SIZE(origin)*sizeof(ObjRef) + sizeof(unsigned int);
	}
	copy = memcpy(heapC, origin, size);
	heapC = heapC + size;
	copiedObjects = copiedObjects + 1;
	copiedObjectSize = copiedObjectSize + size;
	freeSpace = freeSpace - size;
	return copy;
}

void purge(void)
{
	if(isFlipped == 1) {
		memset(&heap[0], 0, heapSize/2);
	}else{
		memset(&heap[heapSize/2], 0, heapSize/2);
	}
}

void gcGoneWild(void)
{
	printf("%s\n", "Garbage Collector:");
	printf("\t%d%s%d%s\n",allocatedObjects, " objects(" ,allocatedObjectSize, " bytes) allocated since last collection");
	printf("\t%d%s%d%s\n", copiedObjects, " objects (",copiedObjectSize ," bytes) copied during this collection");
	printf("\t%d%s%d%s\n", freeSpace, " of ", heapSize/2, " bytes free after this collection");
}