// ===== 00222310 FUN_00222310 (depth 0)

void FUN_00222310(undefined4 param_1)

{
  undefined4 local_20 [4];
  
  FUN_002724f0(local_20,param_1,4);
  FUN_0022233c(local_20[0]);
  return;
}


// ===== 00223190 FUN_00223190 (depth 0)

undefined4 FUN_00223190(undefined4 param_1)

{
  int iVar1;
  
  iVar1 = FUN_001b8ba0(&DAT_003d8bd8,param_1);
  return *(undefined4 *)(iVar1 + 0xc);
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


// ===== 002250c8 FUN_002250c8 (depth 0)

void FUN_002250c8(undefined4 *param_1)

{
  int iVar1;
  uint uVar2;
  
  uVar2 = param_1[0x11];
  *param_1 = &PTR_FUN_0033de88;
  if (((uVar2 >> 2 & 1) != 0) && (param_1[2] != 0)) {
    if (param_1[0xf] != 0) {
      iVar1 = FUN_001b8ba0(&DAT_003d8bd8,param_1[0xf]);
      (**(code **)(**(int **)(iVar1 + 0xc) + 4))(*(int **)(iVar1 + 0xc),param_1[2]);
      uVar2 = param_1[0x11];
    }
    param_1[2] = 0;
  }
  if ((uVar2 >> 6 & 1) == 0) {
    FUN_00220784(param_1[0x10]);
  }
  FUN_00224abc(param_1);
  return;
}


// ===== 002253a8 FUN_002253a8 (depth 0)

void FUN_002253a8(int param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4)

{
  int iVar1;
  undefined4 uVar2;
  undefined4 *puVar3;
  undefined4 local_20 [4];
  
  local_20[0] = param_2;
  iVar1 = FUN_001b8ba0(&DAT_003d8bd8,param_4);
  if (iVar1 == 0) {
    *(undefined4 *)(param_1 + 8) = local_20[0];
  }
  else {
    puVar3 = *(undefined4 **)(iVar1 + 0xc);
    iVar1 = DAT_003f1920 * 4;
    DAT_003f1920 = DAT_003f1920 + 1;
    *(int *)(&DAT_003f1900 + iVar1) = param_1;
    uVar2 = (**(code **)*puVar3)(puVar3,local_20,param_3);
    *(undefined4 *)(param_1 + 8) = uVar2;
    DAT_003f1920 = DAT_003f1920 + -1;
  }
  *(undefined4 *)(param_1 + 0x40) = local_20[0];
  *(uint *)(param_1 + 0x44) = *(uint *)(param_1 + 0x44) | 4;
  *(undefined4 *)(param_1 + 0xc) = 0;
  FUN_00224e70(param_1,1);
  if ((*(byte *)(param_1 + 0x44) & 1) == 0) {
    FUN_00223a58(param_1);
  }
  return;
}


// ===== 0022493c FUN_0022493c (depth 0)

void FUN_0022493c(undefined4 *param_1,undefined4 param_2,undefined1 param_3,undefined1 param_4)

{
  undefined4 uVar1;
  
  *param_1 = &PTR_FUN_0033bc58;
  uVar1 = FUN_00222310(param_2);
  FUN_001b8ab4(param_1 + 1,uVar1,param_1,0);
  *(undefined1 *)((int)param_1 + 0x15) = param_4;
  *(undefined1 *)(param_1 + 5) = param_3;
  FUN_001b8b44(&DAT_003d8bd8,param_1 + 1);
  return;
}


// ===== 001e8464 FUN_001e8464 (depth 0)

void FUN_001e8464(int param_1,uint param_2)

{
  bool bVar1;
  undefined4 *puVar2;
  
  bVar1 = (param_2 ^ 0xffff) == 0;
  if (bVar1 && param_1 == 1) {
    FUN_001b8aec(&DAT_003d8b90,0,0);
    FUN_001b8aec(&DAT_003d8ba8,0,0);
    FUN_001b8aec(&DAT_003d8bc0,0,0);
    FUN_001b8aec(&DAT_003d8bd8,0,0);
    DAT_003d8bf8 = DAT_003d8bf8 & 0xfffffff0;
    DAT_003d8bf0 = &PTR_FUN_0033c420;
    DAT_003d8bf4 = 0;
    DAT_003d8bfc = 0;
    FUN_001bd6d4(&DAT_003d8c00,0,0xffffffff);
    DAT_003d8c18 = 0;
    DAT_003d8c24 = 0;
    DAT_003d8c2c = 0;
    DAT_003d8c10 = 0;
    DAT_003d8c14 = 0;
    DAT_003d8c28 = 0;
    if (bVar1 && param_1 == 1) {
      DAT_003dcc40 = "button_psp_cross_abe.psm";
      DAT_003dcc54 = 0x534f5243;
      DAT_003dcc44 = 0;
      DAT_003dcc48 = 0;
      DAT_003dcc4c = 0;
      DAT_003dcc50 = 0;
      DAT_003dcc58 = 0x18;
      DAT_003dcc5c = 0x18;
      DAT_003dcc60 = "button_psp_circle_abe.psm";
      DAT_003dcc74 = 0x43524943;
      DAT_003dcc64 = 0;
      DAT_003dcc68 = 0;
      DAT_003dcc6c = 0;
      DAT_003dcc70 = 0;
      DAT_003dcc78 = 0x18;
      DAT_003dcc7c = 0x18;
      DAT_003dcc80 = "button_psp_triangle_abe.psm";
      DAT_003dcc94 = 0x41495254;
      DAT_003dcc84 = 0;
      DAT_003dcc88 = 0;
      DAT_003dcc8c = 0;
      DAT_003dcc90 = 0;
      DAT_003dcc98 = 0x18;
      DAT_003dcc9c = 0x18;
      DAT_003dcca0 = "button_psp_square_abe.psm";
      DAT_003dccb4 = 0x41555153;
      DAT_003dcca4 = 0;
      DAT_003dcca8 = 0;
      DAT_003dccac = 0;
      DAT_003dccb0 = 0;
      DAT_003dccb8 = 0x18;
      DAT_003dccbc = 0x18;
      DAT_003dccc0 = "button_psp_r1_abe.psm";
      DAT_003dccc4 = 0;
      DAT_003dccc8 = 0;
      DAT_003dccd4 = 0x5f5f3152;
      DAT_003dcccc = 0;
      DAT_003dccd0 = 0;
      DAT_003dccd8 = 0x20;
      DAT_003dccdc = 0x16;
      DAT_003dcce0 = "button_psp_l1_abe.psm";
      DAT_003dcce4 = 0;
      DAT_003dcce8 = 0;
      DAT_003dccf4 = 0x5f5f314c;
      DAT_003dccf8 = 0x20;
      DAT_003dccfc = 0x16;
      DAT_003dccec = 0;
      DAT_003dccf0 = 0;
      DAT_003dcd00 = "button_psp_up_abe.psm";
      DAT_003dcd04 = 0;
      DAT_003dcd08 = 0;
      DAT_003dcd14 = 0x5f5f5055;
      DAT_003dcd0c = 0;
      DAT_003dcd10 = 0;
      DAT_003dcd18 = 0x18;
      DAT_003dcd1c = 0x18;
      DAT_003dcd20 = "button_psp_down_abe.psm";
      DAT_003dcd24 = 0;
      DAT_003dcd28 = 0;
      DAT_003dcd34 = 0x4e574f44;
      DAT_003dcd2c = 0;
      DAT_003dcd30 = 0;
      DAT_003dcd38 = 0x18;
      DAT_003dcd3c = 0x18;
      DAT_003dcd40 = "button_psp_left_abe.psm";
      DAT_003dcd44 = 0;
      DAT_003dcd48 = 0;
      DAT_003dcd4c = 0;
      DAT_003dcd54 = 0x5446454c;
      DAT_003dcd50 = 0;
      DAT_003dcd58 = 0x18;
      DAT_003dcd5c = 0x18;
      DAT_003dcd60 = "button_psp_right_abe.psm";
      DAT_003dcd74 = 0x48474952;
      DAT_003dcd64 = 0;
      DAT_003dcd68 = 0;
      DAT_003dcd6c = 0;
      DAT_003dcd70 = 0;
      DAT_003dcd78 = 0x18;
      DAT_003dcd7c = 0x18;
      DAT_003dcd80 = "button_psp_analogue_abe.psm";
      DAT_003dcd94 = 0x414e414c;
      DAT_003dcd9c = 0x18;
      DAT_003dcd84 = 0;
      DAT_003dcd88 = 0;
      DAT_003dcd8c = 0;
      DAT_003dcd90 = 0;
      DAT_003dcd98 = 0x18;
      DAT_003dcdbc = 0;
      DAT_003dcda0 = 0;
      DAT_003dcda4 = 0;
      DAT_003dcda8 = 0;
      DAT_003dcdac = 0;
      DAT_003dcdb0 = 0;
      DAT_003dcdb4 = 0;
      DAT_003dcdb8 = 0;
    }
  }
  if (bVar1 && param_1 == 0) {
    puVar2 = (undefined4 *)&DAT_003dcdc0;
    while ((char **)puVar2 != &DAT_003dcc40) {
      FUN_002bff34(puVar2 + -4);
      FUN_0028870c(puVar2 + -6,0);
      FUN_00288780(puVar2 + -6);
      puVar2 = puVar2 + -8;
    }
  }
  if ((param_2 ^ 0xffff) == 0 && param_1 == 0) {
    FUN_0028870c(&DAT_003d8c24,0);
    FUN_00288780(&DAT_003d8c24);
    FUN_001bd748(&DAT_003d8bf0);
    FUN_001b8ac8(&DAT_003d8bdc);
    FUN_001b8ac8(&DAT_003d8bc4);
    FUN_001b8ac8(&DAT_003d8bac);
    FUN_001b8ac8(&DAT_003d8b94);
  }
  return;
}


