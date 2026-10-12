// ===== 0001f678 FUN_0001f678 (depth 0)

void FUN_0001f678(undefined4 *param_1,undefined4 param_2,undefined1 param_3,undefined1 param_4)

{
  undefined4 *puVar1;
  undefined4 uVar2;
  int iVar3;
  char *pcVar4;
  undefined1 auStack_1b0 [64];
  undefined1 auStack_170 [32];
  char local_150 [4];
  char local_14c [4];
  char local_148;
  undefined4 *local_50;
  undefined4 *local_4c;
  undefined4 *local_48;
  undefined4 *local_44;
  undefined4 *local_40;
  undefined4 *local_3c;
  undefined4 *local_38;
  undefined4 *local_34;
  
  FUN_000009e0();
  local_50 = param_1 + 6;
  local_44 = param_1 + 0xc;
  local_48 = param_1 + 10;
  local_4c = param_1 + 8;
  *param_1 = &PTR_LAB_00324f98;
  param_1[6] = 0;
  *(undefined1 *)(param_1 + 7) = 0;
  param_1[8] = 0;
  *(undefined1 *)(param_1 + 9) = 0;
  param_1[10] = 0;
  *(undefined1 *)(param_1 + 0xb) = 0;
  param_1[0xc] = 0;
  *(undefined1 *)(param_1 + 0xd) = 0;
  param_1[0xe] = 0;
  param_1[0xf] = 0;
  param_1[0x10] = 0;
  uVar2 = DAT_0033f300;
  param_1[0x14] = param_2;
  param_1[0x12] = uVar2;
  *(undefined1 *)(param_1 + 0x81) = param_3;
  *(undefined1 *)(param_1 + 0x83) = param_4;
  *(undefined1 *)(param_1 + 0x7c) = 0;
  *(undefined1 *)((int)param_1 + 0x1f1) = 0;
  *(undefined1 *)((int)param_1 + 0x1f2) = 0;
  param_1[0x7d] = 0;
  param_1[0x7e] = 0;
  param_1[0x7f] = 0;
  param_1[0x80] = 0;
  param_1[0x82] = 0;
  DAT_003d8c1c = DAT_003d8c1c | 0x10;
  param_1[0x13] = 0xff;
  param_1[0x11] = 0;
  DAT_003d8c1c = DAT_003d8c1c | 0x200;
  param_1[1] = 0x10;
  *(undefined1 *)(param_1 + 0x74) = 0;
  iVar3 = *(int *)(DAT_003d8834 + 0x30);
  *(undefined1 *)(iVar3 + 3) = 0;
  FUN_000102a4(iVar3);
  puVar1 = (undefined4 *)FUN_001baf40(0x18);
  puVar1[4] = param_1 + 0xe;
  *puVar1 = &PTR_LAB_00325000;
  puVar1[1] = 0;
  puVar1[2] = 0;
  puVar1[3] = param_1;
  *(undefined1 *)(puVar1 + 5) = 0;
  FUN_00289c10(param_1 + 0xe,puVar1);
  puVar1 = (undefined4 *)FUN_001baf40(0x18);
  puVar1[4] = param_1 + 0xf;
  *puVar1 = &PTR_LAB_00325000;
  puVar1[1] = 0;
  puVar1[2] = 0;
  puVar1[3] = param_1;
  *(undefined1 *)(puVar1 + 5) = 0;
  FUN_00289c10(param_1 + 0xf,puVar1);
  puVar1 = (undefined4 *)FUN_001baf40(0x18);
  *puVar1 = &PTR_LAB_00325000;
  puVar1[4] = param_1 + 0x10;
  puVar1[1] = 0;
  puVar1[2] = 0;
  puVar1[3] = param_1;
  *(undefined1 *)(puVar1 + 5) = 0;
  FUN_00289c10(param_1 + 0x10,puVar1);
  local_40 = param_1 + 0x60;
  local_38 = param_1 + 0x6a;
  local_3c = param_1 + 0x65;
  local_34 = param_1 + 0x6f;
  param_1[0x17] = "00_hallofheroes";
  param_1[0x16] = 10;
  param_1[0x18] = "hh_cs_001.pmf";
  param_1[0x1b] = 2;
  param_1[0x1c] = "01_danscrypt";
  param_1[0x1d] = "dc_cs_001.pmf";
  param_1[0x21] = "02_graveyard";
  param_1[0x19] = 0;
  param_1[0x1e] = 0;
  param_1[0x23] = 0;
  param_1[0x28] = "ch_cs_001.pmf";
  param_1[0x20] = 0xb;
  param_1[0x25] = 0xc;
  param_1[0x26] = "03_cemeteryhill";
  param_1[0x2b] = "04_mausoleum";
  param_1[0x2d] = "ma_cs_001.pmf";
  param_1[0x2a] = 0xd;
  param_1[0x2f] = 0xe;
  param_1[0x30] = "05_returntograveyard";
  param_1[0x15] = 3;
  param_1[0x33] = 0xf;
  param_1[0x34] = 0x10;
  param_1[0x35] = "06_plains";
  param_1[0x22] = 0;
  param_1[0x1a] = 2;
  param_1[0x27] = "ch_cs_001.pmf";
  param_1[0x1f] = 0xb;
  param_1[0x32] = 0;
  param_1[0x2c] = "ma_cs_001.pmf";
  param_1[0x24] = 0xc;
  param_1[0x2e] = 0xe;
  param_1[0x3a] = "07_scarecrowfields";
  param_1[0x29] = 0xd;
  param_1[0x39] = 0x14;
  param_1[0x3e] = 0x15;
  param_1[0x3f] = "08_pumpkingorge";
  param_1[0x37] = 0;
  param_1[0x3c] = 0;
  param_1[0x41] = 0;
  param_1[0x46] = "sv_cs_001.pmf";
  param_1[0x4b] = "ia_cs_001.pmf";
  param_1[0x43] = 0x11;
  param_1[0x48] = 0x13;
  param_1[0x4d] = 0x12;
  param_1[0x52] = 0x16;
  param_1[0x44] = "09_sleepingvillage";
  param_1[0x57] = 0x18;
  param_1[0x49] = "10_asylum";
  param_1[0x4e] = "09_sleepingvillage";
  param_1[0x53] = "11_enchantedforest";
  param_1[0x31] = 0;
  param_1[0x38] = 0x14;
  param_1[0x36] = 0;
  param_1[0x3b] = 0;
  param_1[0x40] = 0;
  param_1[0x45] = "sv_cs_001.pmf";
  param_1[0x3d] = 0x15;
  param_1[0x4a] = "ia_cs_001.pmf";
  param_1[0x42] = 0x11;
  param_1[0x50] = 0;
  param_1[0x4f] = 0;
  param_1[0x55] = 0;
  param_1[0x47] = 0x13;
  param_1[0x4c] = 0x12;
  param_1[0x56] = 0x18;
  param_1[0x58] = "14_scurvydocks";
  param_1[0x5c] = 0x19;
  param_1[0x51] = 0x16;
  param_1[0x54] = 0;
  param_1[0x5a] = 0;
  param_1[0x5d] = "15_dragonisland";
  param_1[0x5f] = "di_cs_001.pmf";
  param_1[0x61] = 0x17;
  param_1[0x62] = "12_poolsofthedead";
  param_1[100] = 0;
  param_1[0x69] = "hr_cs_005.pmf";
  param_1[0x6e] = "gs_cs_001.pmf";
  param_1[0x73] = "zl_cs_001.pmf";
  param_1[0x66] = 0x1a;
  param_1[0x67] = "16_hauntedruins";
  param_1[0x6b] = 0x1b;
  param_1[0x6c] = "17_ghostship";
  param_1[0x59] = 0;
  param_1[0x70] = 0x1c;
  param_1[0x71] = "22_zarokslair";
  param_1[0x5b] = 0x19;
  param_1[0x60] = 0x17;
  param_1[0x5e] = "di_cs_001.pmf";
  param_1[99] = 0;
  param_1[0x65] = 0x1a;
  param_1[0x68] = "hr_cs_005.pmf";
  param_1[0x6a] = 0x1b;
  param_1[0x6d] = "gs_cs_001.pmf";
  param_1[0x6f] = 0x1c;
  param_1[0x72] = "zl_cs_001.pmf";
  FUN_00222740(&DAT_0034d600,"inventory_backdrop.psm",local_50,0xffffffff,0,0,0,0);
  FUN_00222740(&DAT_0034d600,"umd_small_spin_bit.psm",local_48,0xffffffff,0,param_1[0xf],0,0);
  FUN_00222740(&DAT_0034d600,"umd_big_spin_bit.psm",local_44,0xffffffff,0,param_1[0x10],0,0);
  if ((uint)param_1[0x14] < 0x13) {
    FUN_00272004(auStack_1b0,"%s.psm",param_1[param_1[0x14] * 5 + 0x17]);
    FUN_00222740(&DAT_0034d600,auStack_1b0,local_4c,0xffffffff,0,param_1[0xe],0,0);
    if (*(char *)(param_1 + 0x81) == '\0') {
      DAT_0034cc04 = DAT_0034cc04 * 0x41c64e6d + 0x3039;
      iVar3 = (param_1[param_1[0x14] * 5 + 0x16] - param_1[param_1[0x14] * 5 + 0x15]) + 1;
      if (iVar3 == 0) {
        trap(7);
      }
      FUN_00272004(auStack_170,"e_000%02d_na.at3",
                   param_1[param_1[0x14] * 5 + 0x15] + (int)(DAT_0034cc04 & 0x7fffffff) % iVar3);
      FUN_002322c0(auStack_170,&DAT_00335441,param_1 + 0x74,0x20);
    }
    iVar3 = FUN_001baf40(0x118);
    FUN_0023489c(0,0,iVar3,400,100,0,0,0,0,0xffff);
    FUN_00236df8(iVar3,7);
    param_1[0x7f] = iVar3;
    *(undefined4 *)(iVar3 + 0xc) = DAT_0034d7c4;
    uVar2 = FUN_0000f880(param_1[0x14]);
    local_148 = s_loading__002c9960[8];
    local_150[0] = s_loading__002c9960[0];
    local_150[1] = s_loading__002c9960[1];
    local_150[2] = s_loading__002c9960[2];
    local_150[3] = s_loading__002c9960[3];
    local_14c[0] = s_loading__002c9960[4];
    local_14c[1] = s_loading__002c9960[5];
    local_14c[2] = s_loading__002c9960[6];
    local_14c[3] = s_loading__002c9960[7];
    FUN_00272104(local_150,uVar2);
    uVar2 = FUN_00231fbc(DAT_003f1988,local_150);
    FUN_00236f5c(param_1[0x7f],uVar2,1);
    FUN_00234ab0(param_1[0x7f],*(uint *)(param_1[0x7f] + 4) | 1);
    if (*(char *)(param_1 + 0x81) == '\0') {
      iVar3 = FUN_0000a5a0(DAT_003d8834,param_1[0x14]);
      if (iVar3 == 0) {
        uVar2 = param_1[param_1[0x14] * 5 + 0x18];
      }
      else {
        uVar2 = param_1[param_1[0x14] * 5 + 0x19];
      }
      param_1[0x80] = uVar2;
    }
  }
  else {
    iVar3 = FUN_001baf40(0x118);
    FUN_0023489c(0,0,iVar3,300,0x32,0,0,0,0,0xffff);
    FUN_00236df8(iVar3,7);
    param_1[0x7e] = iVar3;
    *(undefined4 *)(iVar3 + 0xc) = DAT_0034d7c4;
    uVar2 = FUN_00231fbc(DAT_003f1988,"loading");
    FUN_00236f5c(param_1[0x7e],uVar2,1);
    FUN_00234ab0(param_1[0x7e],*(uint *)(param_1[0x7e] + 4) | 1);
    if (DAT_00367928 == '\0') {
      pcVar4 = "medievil.psm";
    }
    else {
      pcVar4 = "quit.psm";
    }
    FUN_00222740(&DAT_0034d600,pcVar4,local_4c,0xffffffff,0,param_1[0xe],0,0);
    *(undefined1 *)((int)param_1 + 0x1f1) = 1;
    *(undefined1 *)(param_1 + 0x7c) = 1;
  }
  DAT_00367928 = 0;
  DAT_00367929 = 1;
  DAT_003d8830 = DAT_003d8830 + 1;
  return;
}


// ===== 00289c10 FUN_00289c10 (depth 1)

int * FUN_00289c10(int *param_1,int *param_2)

{
  int *piVar1;
  
  piVar1 = (int *)*param_1;
  if (param_2 != piVar1) {
    if (param_2 != (int *)0x0) {
      param_2[1] = param_2[1] + 1;
      piVar1 = (int *)*param_1;
    }
    if (piVar1 == (int *)0x0) {
      *param_1 = (int)param_2;
    }
    else {
      (**(code **)(*piVar1 + 8))(piVar1);
      *param_1 = (int)param_2;
    }
  }
  return param_2;
}


// ===== 00236f5c FUN_00236f5c (depth 1)

void FUN_00236f5c(int param_1,undefined4 param_2,undefined1 param_3)

{
  undefined4 uVar1;
  
  if (*(int *)(param_1 + 0x104) != 0) {
    FUN_00236e10();
  }
  uVar1 = FUN_001baf40(4);
  FUN_001b6448(uVar1,param_2);
  *(undefined1 *)(param_1 + 0x114) = param_3;
  *(undefined4 *)(param_1 + 0x104) = uVar1;
  *(undefined1 *)(param_1 + 0x108) = 1;
  return;
}


// ===== 0000f880 FUN_0000f880 (depth 1)

int FUN_0000f880(int param_1)

{
  int iVar1;
  
  iVar1 = (&DAT_0033eda0)[param_1 * 3] + 4;
  if ((&DAT_0033eda0)[param_1 * 3] == 0) {
    iVar1 = 0;
  }
  return iVar1;
}


// ===== 00234ab0 FUN_00234ab0 (depth 1)

void FUN_00234ab0(int param_1,uint param_2)

{
  if ((param_2 & 0x7800) != (*(uint *)(param_1 + 4) & 0x7800)) {
    FUN_00234af8();
  }
  *(uint *)(param_1 + 4) = param_2;
  return;
}


// ===== 00272004 FUN_00272004 (depth 1)

void FUN_00272004(undefined1 *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5,undefined4 param_6,undefined4 param_7,undefined4 param_8)

{
  undefined1 *local_90 [2];
  undefined4 local_88;
  undefined2 local_84;
  undefined2 local_82;
  undefined1 *local_80;
  undefined4 local_7c;
  undefined4 local_18;
  undefined4 local_14;
  undefined4 local_10;
  undefined4 local_c;
  undefined4 local_8;
  undefined4 local_4;
  
  local_84 = 0x208;
  local_7c = 0x7fffffff;
  local_88 = 0x7fffffff;
  local_82 = 0xffff;
  local_90[0] = param_1;
  local_80 = param_1;
  local_18 = param_3;
  local_14 = param_4;
  local_10 = param_5;
  local_c = param_6;
  local_8 = param_7;
  local_4 = param_8;
  FUN_00273b04(PTR_DAT_0034f840,local_90,param_2,&local_18);
  *local_90[0] = 0;
  return;
}


// ===== 000009e0 FUN_000009e0 (depth 1)

void FUN_000009e0(undefined4 *param_1)

{
  *param_1 = &DAT_003246a0;
  param_1[1] = 0xffffffff;
  param_1[5] = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  param_1[4] = 0;
  return;
}


// ===== 000102a4 FUN_000102a4 (depth 1)

void FUN_000102a4(int param_1)

{
  int iVar1;
  
  iVar1 = *(int *)(param_1 + 0x40);
  if (iVar1 == 0) {
    *(undefined4 *)(param_1 + 0x40) = 0;
  }
  else {
    FUN_0003d084(iVar1);
    FUN_001bb13c(iVar1);
    *(undefined4 *)(param_1 + 0x40) = 0;
  }
  return;
}


// ===== 001baf40 FUN_001baf40 (depth 1)

void FUN_001baf40(undefined4 param_1)

{
  DAT_003985b4 = DAT_003985b4 + 1;
  FUN_001bbd88(&DAT_00398560,param_1);
  return;
}


// ===== 00236df8 FUN_00236df8 (depth 1)

void FUN_00236df8(int param_1,undefined4 param_2)

{
  *(undefined4 *)(param_1 + 0x110) = param_2;
  *(undefined4 *)(param_1 + 0x10c) = 0;
  *(undefined1 *)(param_1 + 0x114) = 0;
  *(undefined4 *)(param_1 + 0x104) = 0;
  *(undefined1 *)(param_1 + 0x108) = 0;
  return;
}


// ===== 0000a5a0 FUN_0000a5a0 (depth 1)

uint FUN_0000a5a0(int param_1,int param_2)

{
  uint uVar1;
  
  uVar1 = 0;
  if (param_2 - 1U < 0x12) {
    uVar1 = FUN_0003c184(param_2);
    uVar1 = (uint)((*(uint *)(param_1 + 0x6c) & uVar1) != 0);
  }
  else if (param_2 == 0) {
    uVar1 = *(uint *)(param_1 + 0x6c) >> 0x12 & 1;
  }
  return uVar1;
}


// ===== 00231fbc FUN_00231fbc (depth 1)

undefined * FUN_00231fbc(int param_1,undefined4 param_2)

{
  undefined4 uVar1;
  undefined4 *puVar2;
  int iVar3;
  undefined *puVar4;
  int iVar5;
  
  uVar1 = FUN_001be1e4(0,param_2,0);
  puVar2 = (undefined4 *)FUN_001b4b34(*(undefined4 *)(param_1 + 8),uVar1);
  if (puVar2 == &DAT_003982ec) {
    iVar5 = 0;
  }
  else {
    iVar3 = FUN_002c53bc(param_1 + 4,param_2,puVar2);
    iVar5 = 0;
    if (iVar3 != 0) {
      iVar5 = *(int *)(iVar3 + 8);
    }
  }
  puVar4 = &DAT_0031910c;
  if (iVar5 != 0) {
    puVar4 = *(undefined **)(iVar5 + 8);
  }
  return puVar4;
}


// ===== 002322c0 FUN_002322c0 (depth 1)

void FUN_002322c0(char *param_1,undefined *param_2,char *param_3)

{
  char *pcVar1;
  undefined4 uVar2;
  char *pcVar3;
  
  pcVar3 = param_1;
  if (*param_1 != '\0' && *param_1 != '.') {
    do {
      pcVar3 = pcVar3 + 1;
    } while (*pcVar3 != '\0' && *pcVar3 != '.');
  }
  if (param_2 != (undefined *)0x434146) {
    if (param_2 < (undefined *)0x434147) {
      if (param_2 != &DAT_00335441) {
        return;
      }
    }
    else if (param_2 != (undefined *)0x4b5056) {
      if (param_2 != (undefined *)0x52544c) {
        return;
      }
      FUN_00272324(param_3,param_1);
      param_3 = pcVar3 + ((int)param_3 - (int)param_1);
      pcVar1 = "_%s%s";
      uVar2 = *(undefined4 *)(*(int *)(DAT_003f1988 + 0x34) + 0x18);
      goto LAB_00232378;
    }
  }
  pcVar3 = param_1 + 1;
  uVar2 = *(undefined4 *)(*(int *)(DAT_003f1988 + 0x38) + 0x18);
  pcVar1 = "%s%s";
LAB_00232378:
  FUN_00272004(param_3,pcVar1,uVar2,pcVar3);
  return;
}


// ===== 00222740 FUN_00222740 (depth 1)

void FUN_00222740(undefined4 param_1,int param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5,undefined4 param_6,undefined1 param_7,undefined1 param_8)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 uVar3;
  int iVar4;
  undefined4 uVar5;
  undefined1 auStack_140 [256];
  undefined4 local_40;
  undefined4 local_3c;
  
  if (*(char *)(param_2 + 1) == ':') {
    param_2 = param_2 + 2;
  }
  local_40 = param_5;
  local_3c = param_6;
  iVar1 = FUN_002223e4(param_2);
  uVar2 = FUN_0022223c(param_2);
  uVar3 = FUN_001be1e4(0,uVar2,0);
  iVar4 = FUN_00232200(iVar1);
  uVar2 = uVar3;
  uVar5 = 0;
  if (iVar4 != 0) {
    FUN_002322c0(param_2,iVar1,auStack_140,0x100);
    uVar2 = FUN_001be1e4(0,auStack_140,0);
    uVar5 = uVar3;
  }
  uVar3 = uVar2;
  if (iVar1 == 0x4d494e41) {
    FUN_00272324(auStack_140,param_2);
    FUN_00272104(auStack_140,".comp");
    uVar3 = FUN_001be1e4(0,auStack_140,0);
    uVar5 = uVar2;
  }
  FUN_002228b4(param_1,param_2,uVar3,uVar5,iVar1,param_3,param_4,local_40,local_3c,param_7,param_8,0
               ,0);
  return;
}


// ===== 00272104 FUN_00272104 (depth 1)

uint * FUN_00272104(uint *param_1)

{
  char cVar1;
  uint uVar2;
  uint *puVar3;
  
  puVar3 = param_1;
  if (((uint)param_1 & 3) == 0) {
    uVar2 = *param_1 + 0xfefefeff & ~*param_1;
    while ((uVar2 & 0x80808080) == 0) {
      puVar3 = puVar3 + 1;
      uVar2 = *puVar3 + 0xfefefeff & ~*puVar3;
    }
  }
  cVar1 = (char)*puVar3;
  while (cVar1 != '\0') {
    puVar3 = (uint *)((int)puVar3 + 1);
    cVar1 = *(char *)puVar3;
  }
  FUN_00272324();
  return param_1;
}


// ===== 0023489c FUN_0023489c (depth 1)

void FUN_0023489c(undefined4 param_1,undefined4 param_2,undefined1 *param_3,undefined4 param_4,
                 undefined4 param_5,undefined4 param_6,undefined4 param_7,int param_8,int param_9,
                 undefined4 param_10)

{
  undefined4 uVar1;
  
  *param_3 = 0;
  if (param_8 == 0 || param_9 == 0) {
    *(undefined4 *)(param_3 + 0xf4) = 0;
  }
  else {
    uVar1 = FUN_001baf40(0x20);
    FUN_0023471c(uVar1,param_6,param_7,param_8,param_9);
    *(undefined4 *)(param_3 + 0xf4) = uVar1;
  }
  *(undefined4 *)(param_3 + 0xc) = 0x80ffffff;
  *(undefined4 *)(param_3 + 0x44) = 10;
  *(undefined4 *)(param_3 + 4) = 0;
  FUN_00234a5c(param_1,param_2,param_3,param_4,param_5);
  FUN_002349d0(DAT_00324110,DAT_00324110,param_3);
  uVar1 = DAT_00324114;
  *(undefined4 *)(param_3 + 0xf8) = param_10;
  *(undefined4 *)(param_3 + 0x100) = uVar1;
  *(undefined4 *)(param_3 + 0xfc) = uVar1;
  return;
}


