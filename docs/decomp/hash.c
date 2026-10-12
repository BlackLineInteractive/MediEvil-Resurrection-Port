// ===== 001be1e4 FUN_001be1e4 (depth 0)

int FUN_001be1e4(int param_1,char *param_2,int param_3)

{
  if (param_3 == 0) {
    param_3 = FUN_002723bc(param_2);
  }
  for (; 0 < param_3; param_3 = param_3 + -1) {
    param_1 = param_1 * 0x81 + (int)*param_2;
    param_2 = param_2 + 1;
  }
  return param_1;
}


// ===== 0022223c FUN_0022223c (depth 0)

char * FUN_0022223c(char *param_1)

{
  char cVar1;
  int iVar2;
  char *pcVar3;
  
  iVar2 = FUN_002723bc();
  pcVar3 = param_1 + iVar2 + -1;
  if (((param_1 <= pcVar3) && (cVar1 = param_1[iVar2 + -1], cVar1 != ':')) &&
     (cVar1 != '/' && cVar1 != '\\')) {
    for (pcVar3 = param_1 + iVar2 + -2; param_1 <= pcVar3; pcVar3 = pcVar3 + -1) {
      cVar1 = *pcVar3;
      if ((cVar1 == ':') || (cVar1 == '/' || cVar1 == '\\')) break;
    }
  }
  return pcVar3 + 1;
}


// ===== 002223e4 FUN_002223e4 (depth 0)

undefined4 FUN_002223e4(void)

{
  undefined4 uVar1;
  int iVar2;
  
  uVar1 = FUN_00222380();
  iVar2 = FUN_001b8ba0(&DAT_003d8bd8,uVar1);
  if (iVar2 == 0) {
    uVar1 = 0;
  }
  return uVar1;
}


// ===== 002723bc FUN_002723bc (depth 1)

int FUN_002723bc(char *param_1)

{
  char cVar1;
  char *pcVar2;
  
  pcVar2 = param_1 + 1;
  do {
    cVar1 = *param_1;
    param_1 = param_1 + 1;
  } while (cVar1 != '\0');
  return (int)param_1 - (int)pcVar2;
}


// ===== 001b8ba0 FUN_001b8ba0 (depth 1)

int FUN_001b8ba0(int param_1,int param_2)

{
  int iVar1;
  
  iVar1 = FUN_001b8bf0();
  if ((iVar1 == param_1 + 4) || (*(int *)(iVar1 + 8) != param_2)) {
    iVar1 = 0;
  }
  return iVar1;
}


// ===== 00222380 FUN_00222380 (depth 1)

undefined4 FUN_00222380(char *param_1)

{
  char *pcVar1;
  char *pcVar2;
  int iVar3;
  undefined4 uVar4;
  
  iVar3 = FUN_002723bc();
  pcVar2 = param_1 + iVar3;
  do {
    pcVar1 = pcVar2;
    if (pcVar1 < param_1) {
      return 0;
    }
    pcVar2 = pcVar1 + -1;
  } while (pcVar1[-1] != '.');
  uVar4 = FUN_00222310(pcVar1);
  return uVar4;
}


