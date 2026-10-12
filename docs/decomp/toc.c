// ===== 0020aba8 FUN_0020aba8 (depth 0)

void FUN_0020aba8(undefined4 *param_1,uint *param_2,char param_3,undefined1 param_4)

{
  undefined4 uVar1;
  uint uVar2;
  uint uVar3;
  uint *puVar4;
  
  param_1[1] = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  *param_1 = &PTR_LAB_0033d990;
  if (param_2 != (uint *)0x0) {
    uVar3 = *param_2;
    do {
      puVar4 = param_2;
      if (uVar3 == 0x3050414d) {
        puVar4 = (uint *)param_2[3];
      }
      if (*puVar4 == 0x53444150) {
LAB_0020acc0:
        param_2 = (uint *)param_2[4];
      }
      else {
        uVar3 = 0xffffffff;
        if (*(byte *)((&DAT_003f18fc)[DAT_003f1920] + 0x34) != 0xff) {
          uVar3 = (uint)*(byte *)((&DAT_003f18fc)[DAT_003f1920] + 0x34);
        }
        uVar1 = FUN_001b9c88(param_1,*puVar4,uVar3);
        uVar3 = *puVar4;
        if (uVar3 == 0x30534d45) {
          FUN_0020e214(puVar4,uVar1,param_4);
          param_2 = (uint *)param_2[4];
        }
        else if ((int)uVar3 < 0x30534d46) {
          if (uVar3 == 0x304e504c) {
            FUN_0020deec(puVar4,uVar1,param_4);
            param_2 = (uint *)param_2[4];
          }
          else if ((int)uVar3 < 0x304e504d) {
            if (uVar3 == 0x30434f4c) {
              param_2 = (uint *)param_2[4];
            }
            else {
              uVar2 = 0x30474f4c;
LAB_0020aca8:
              if (uVar3 == uVar2) {
                param_2 = (uint *)param_2[4];
              }
              else {
                if (DAT_003dd9a0 == FUN_00000000) goto LAB_0020acc0;
                (*DAT_003dd9a0)(puVar4,uVar1,param_4);
                param_2 = (uint *)param_2[4];
              }
            }
          }
          else {
            if (uVar3 != 0x30524f4c) {
              uVar2 = 0x30530000;
LAB_0020ad74:
              uVar2 = uVar2 | 0x415a;
              goto LAB_0020aca8;
            }
            FUN_0020aacc(puVar4,uVar1,param_4);
            param_2 = (uint *)param_2[4];
          }
        }
        else if (uVar3 == 0x31434c54) {
          param_2 = (uint *)param_2[4];
        }
        else if ((int)uVar3 < 0x31434c55) {
          if (uVar3 != 0x3053504c) {
            uVar2 = 0x30554c54;
            goto LAB_0020aca8;
          }
          FUN_0020e0bc(puVar4,uVar1,param_4);
          param_2 = (uint *)param_2[4];
        }
        else {
          if (uVar3 != 0x31434f4c) {
            uVar2 = 0x31530000;
            goto LAB_0020ad74;
          }
          FUN_0020a974(puVar4,uVar1,param_4);
          param_2 = (uint *)param_2[4];
        }
      }
      if (param_2 == (uint *)0x0) break;
      uVar3 = *param_2;
    } while( true );
  }
  if (param_3 == '\0') {
    uVar3 = 0xffffffff;
    if (*(byte *)((&DAT_003f18fc)[DAT_003f1920] + 0x34) != 0xff) {
      uVar3 = (uint)*(byte *)((&DAT_003f18fc)[DAT_003f1920] + 0x34);
    }
    puVar4 = (uint *)(*(int *)(*(int *)(*(int *)(*(int *)(DAT_003d8834 + 4) + 0x30) + 8) + 8) +
                     uVar3 * 0x280);
    *puVar4 = *puVar4 | 0x100;
  }
  return;
}


// ===== 0020ae1c FUN_0020ae1c (depth 0)

void FUN_0020ae1c(undefined4 *param_1,uint *param_2,char param_3,undefined1 param_4)

{
  undefined4 uVar1;
  uint uVar2;
  uint uVar3;
  uint *puVar4;
  
  param_1[1] = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  *param_1 = &PTR_LAB_0033d990;
  if (param_2 != (uint *)0x0) {
    uVar3 = *param_2;
    do {
      puVar4 = param_2;
      if (uVar3 == 0x3050414d) {
        puVar4 = (uint *)param_2[3];
      }
      if (*puVar4 == 0x53444150) {
LAB_0020af34:
        param_2 = (uint *)param_2[4];
      }
      else {
        uVar3 = 0xffffffff;
        if (*(byte *)((&DAT_003f18fc)[DAT_003f1920] + 0x34) != 0xff) {
          uVar3 = (uint)*(byte *)((&DAT_003f18fc)[DAT_003f1920] + 0x34);
        }
        uVar1 = FUN_001b9c88(param_1,*puVar4,uVar3);
        uVar3 = *puVar4;
        if (uVar3 == 0x30534d45) {
          FUN_0020e214(puVar4,uVar1,param_4);
          param_2 = (uint *)param_2[4];
        }
        else if ((int)uVar3 < 0x30534d46) {
          if (uVar3 == 0x304e504c) {
            FUN_0020deec(puVar4,uVar1,param_4);
            param_2 = (uint *)param_2[4];
          }
          else if ((int)uVar3 < 0x304e504d) {
            if (uVar3 == 0x30434f4c) {
              param_2 = (uint *)param_2[4];
            }
            else {
              uVar2 = 0x30474f4c;
LAB_0020af1c:
              if (uVar3 == uVar2) {
                param_2 = (uint *)param_2[4];
              }
              else {
                if (DAT_003dd9a0 == FUN_00000000) goto LAB_0020af34;
                (*DAT_003dd9a0)(puVar4,uVar1,param_4);
                param_2 = (uint *)param_2[4];
              }
            }
          }
          else {
            if (uVar3 != 0x30524f4c) {
              uVar2 = 0x30530000;
LAB_0020afe8:
              uVar2 = uVar2 | 0x415a;
              goto LAB_0020af1c;
            }
            FUN_0020aacc(puVar4,uVar1,param_4);
            param_2 = (uint *)param_2[4];
          }
        }
        else if (uVar3 == 0x31434c54) {
          param_2 = (uint *)param_2[4];
        }
        else if ((int)uVar3 < 0x31434c55) {
          if (uVar3 != 0x3053504c) {
            uVar2 = 0x30554c54;
            goto LAB_0020af1c;
          }
          FUN_0020e0bc(puVar4,uVar1,param_4);
          param_2 = (uint *)param_2[4];
        }
        else {
          if (uVar3 != 0x31434f4c) {
            uVar2 = 0x31530000;
            goto LAB_0020afe8;
          }
          FUN_0020a974(puVar4,uVar1,param_4);
          param_2 = (uint *)param_2[4];
        }
      }
      if (param_2 == (uint *)0x0) break;
      uVar3 = *param_2;
    } while( true );
  }
  if (param_3 == '\0') {
    uVar3 = 0xffffffff;
    if (*(byte *)((&DAT_003f18fc)[DAT_003f1920] + 0x34) != 0xff) {
      uVar3 = (uint)*(byte *)((&DAT_003f18fc)[DAT_003f1920] + 0x34);
    }
    puVar4 = (uint *)(*(int *)(*(int *)(*(int *)(*(int *)(DAT_003d8834 + 4) + 0x30) + 8) + 8) +
                     uVar3 * 0x280);
    *puVar4 = *puVar4 | 0x100;
  }
  return;
}


// ===== 0020b090 FUN_0020b090 (depth 0)

void FUN_0020b090(undefined4 *param_1)

{
  int iVar1;
  int iVar2;
  int iVar3;
  int *piVar4;
  int local_4c;
  
  piVar4 = param_1 + 1;
  iVar2 = param_1[1];
  *param_1 = &PTR_LAB_0033d990;
  if (iVar2 == 0) {
LAB_0020b1bc:
    *param_1 = &PTR_LAB_0033c368;
    if (param_1[2] != 0) {
      iVar2 = *piVar4;
      while( true ) {
        FUN_002bc0c0(piVar4,iVar2,iVar2);
        if (iVar2 != 0) {
          *(undefined4 *)(iVar2 + 0x10) = 0;
          *(undefined4 *)(iVar2 + 0x14) = 0;
          FUN_001bb13c(iVar2);
        }
        if (param_1[2] == 0) break;
        iVar2 = *piVar4;
      }
    }
    return;
  }
  iVar1 = *(int *)(iVar2 + 4);
  local_4c = iVar2;
  do {
    if (iVar1 == 0x30534d45) {
LAB_0020b264:
      FUN_0020aa50();
      iVar3 = local_4c;
    }
    else {
      iVar3 = iVar2;
      if (iVar1 < 0x30534d46) {
        if (iVar1 == 0x304e504c) {
          FUN_0020dfc8();
          iVar3 = local_4c;
        }
        else if (iVar1 < 0x304e504d) {
          if ((iVar1 != 0x30434f4c) && (iVar1 != 0x30474f4c)) goto LAB_0020b178;
        }
        else {
          if (iVar1 == 0x30524f4c) goto LAB_0020b264;
          if (iVar1 != 0x3053415a) goto LAB_0020b178;
        }
      }
      else if (iVar1 != 0x31434c54) {
        if (iVar1 < 0x31434c55) {
          if (iVar1 == 0x3053504c) {
            FUN_0020e198();
            iVar3 = local_4c;
          }
          else if (iVar1 != 0x30554c54) goto LAB_0020b178;
        }
        else {
          if ((iVar1 == 0x31434f4c) || (iVar1 == 0x3153415a)) goto LAB_0020b264;
LAB_0020b178:
          if (DAT_003dd9a4 != FUN_00000000) {
            (*DAT_003dd9a4)();
            iVar3 = local_4c;
          }
        }
      }
    }
    iVar1 = *(int *)(iVar3 + param_1[3]);
    iVar2 = iVar3;
    if (iVar1 != 0) {
      iVar2 = iVar1;
      local_4c = iVar1;
    }
    if (iVar3 == iVar2) goto LAB_0020b1bc;
    iVar1 = *(int *)(iVar2 + 4);
  } while( true );
}


// ===== 002226d8 FUN_002226d8 (depth 0)

void FUN_002226d8(undefined4 param_1,int param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5,undefined4 param_6,undefined4 param_7,undefined1 param_8,
                 undefined1 uStackX_0)

{
  if (*(char *)(param_2 + 1) == ':') {
    param_2 = param_2 + 2;
  }
  FUN_002228b4(param_1,param_2,param_3,0,0x554f5247,param_4,param_5,param_6,param_7,0,param_8,0,
               uStackX_0);
  return;
}


// ===== 002228b4 FUN_002228b4 (depth 0)

int FUN_002228b4(int param_1,undefined4 param_2,int param_3,int param_4,int param_5,int param_6,
                undefined4 param_7,int *param_8,int *piStackX_0,byte bStackX_4,undefined4 uStackX_8,
                byte bStackX_c,byte param_13)

{
  short sVar1;
  bool bVar2;
  char cVar3;
  ushort uVar4;
  int iVar5;
  int iVar6;
  undefined4 *puVar7;
  undefined4 *puVar8;
  undefined4 *puVar9;
  int iVar10;
  int *piVar11;
  uint uVar12;
  undefined4 local_70 [4];
  int local_60;
  int local_5c;
  int local_58;
  undefined4 local_54;
  uint local_50;
  uint local_4c;
  int local_48;
  uint local_44;
  uint local_40;
  int local_3c;
  int local_38;
  
  local_44 = (uint)(param_5 == 0x554f5247);
  local_50 = (uint)bStackX_c;
  local_4c = (uint)param_13;
  piVar11 = (int *)0x0;
  uVar12 = (uint)bStackX_4;
  local_48 = 0;
  local_5c = param_1;
  local_58 = param_3;
  local_54 = param_7;
  if (local_44 == 0) {
    iVar5 = FUN_00223190(param_5);
    if (uVar12 == 0) {
      uVar12 = (uint)*(byte *)(iVar5 + 0x14);
      goto LAB_0022293c;
    }
LAB_00222944:
    iVar5 = FUN_00222380(param_2);
    if (((iVar5 != 0x474b50) && (*(int *)(local_5c + 0x60) != 0)) && (local_44 == 0)) {
      puVar7 = (undefined4 *)FUN_00223f98(*(int *)(local_5c + 0x60),local_58);
      local_40 = (uint)(param_4 != 0);
      if (puVar7 == (undefined4 *)0x0 && param_4 != 0) {
        puVar7 = (undefined4 *)FUN_00223f98(*(undefined4 *)(local_5c + 0x60),param_4);
      }
      if (((puVar7 != (undefined4 *)0x0) && (puVar7[2] != 0)) &&
         ((uVar12 == 0 || (*(char *)(puVar7 + 3) == '\0')))) {
        FUN_00222fec(local_5c,puVar7[2],puVar7[1],*puVar7,param_5,param_2,0,local_54,
                     *(char *)(puVar7 + 3),0);
        puVar9 = (undefined4 *)FUN_001b4b34(DAT_0034d62c,local_58);
        piVar11 = (int *)0x0;
        if (puVar9 != &DAT_003982ec) {
          piVar11 = (int *)puVar9[2];
        }
        if ((piVar11 == (int *)0x0 & local_40) != 0) {
          puVar9 = (undefined4 *)FUN_001b4b34(DAT_0034d62c,param_4);
          piVar11 = (int *)0x0;
          if (puVar9 != &DAT_003982ec) {
            piVar11 = (int *)puVar9[2];
          }
        }
        puVar7[2] = 0;
        if (piVar11 != (int *)0x0) {
          local_48 = 1;
        }
      }
    }
    if (piVar11 == (int *)0x0) {
      local_3c = local_58;
      if ((param_4 != 0) && (cVar3 = FUN_00221558(&DAT_0034cc40,local_58), cVar3 == '\0')) {
        local_3c = param_4;
      }
      if ((local_50 == 0) && (cVar3 = FUN_00221558(&DAT_0034cc40,local_3c), cVar3 == '\0')) {
        if (piStackX_0 == (int *)0x0) {
          return 0;
        }
        *(undefined1 *)(piStackX_0 + 5) = 1;
        return 0;
      }
      if (param_8 == (int *)0x0) {
        param_8 = (int *)FUN_00222430();
      }
      if (local_44 == 0) {
        piVar11 = (int *)FUN_001baf40(0x48);
        FUN_00225030(piVar11,param_2,local_3c,param_5,param_8,piStackX_0);
        piVar11[0x11] = piVar11[0x11] & 0xfffffffeU | uVar12 & 1;
        if (local_50 != 0) {
          piVar11[0xc] = *(int *)(*(int *)(local_5c + 0xc) + 0x30);
        }
      }
      else {
        piVar11 = (int *)FUN_001baf40(0x4c);
        FUN_00225518(piVar11,param_2,local_3c,param_8,piStackX_0,local_4c);
      }
      if (param_6 != 0) {
        piVar11[5] = piVar11[5] & 0xfffffffeU | *(byte *)(param_6 + 4) & 1;
      }
      *(char *)(piVar11 + 0xd) = (char)local_54;
      piVar11[5] = piVar11[5] | 0x10;
      piVar11[1] = piVar11[1] + 1;
      *(int **)((int)piVar11 + DAT_0034d608) = DAT_0034d600;
      DAT_0034d604 = DAT_0034d604 + 1;
      DAT_0034d600 = piVar11;
      if (local_44 != 0) {
        puVar7 = (undefined4 *)FUN_00221878(&DAT_0034cc40,local_3c,&local_60,1);
        local_38 = 1;
        if (puVar7 != (undefined4 *)0x0) {
          iVar6 = 0;
          iVar5 = local_60;
          puVar9 = puVar7;
          if (0 < local_60) {
            do {
              iVar6 = iVar6 + 1;
              if (puVar9[4] == 0xfffffe) break;
              uVar4 = *(ushort *)(puVar9 + 3) & 0xf;
              if (uVar4 == 5) {
                bVar2 = iVar6 < iVar5;
              }
              else if (uVar4 == 6) {
                bVar2 = iVar6 < iVar5;
              }
              else if (uVar4 == 2) {
                bVar2 = iVar6 < iVar5;
              }
              else {
                if (uVar4 != 6) {
                  puVar8 = (undefined4 *)FUN_001b4b34(DAT_0034d62c,*puVar9);
                  iVar10 = 0;
                  if (puVar8 != &DAT_003982ec) {
                    iVar10 = puVar8[2];
                  }
                  iVar5 = local_60;
                  if (iVar10 == 0) {
                    local_38 = 0;
                    break;
                  }
                }
                bVar2 = iVar6 < iVar5;
              }
              puVar9 = puVar9 + 6;
            } while (bVar2);
          }
          if (puVar7 != (undefined4 *)0x0) {
            FUN_001bb178(puVar7);
          }
          if (local_38 == 0) goto LAB_00222a1c;
        }
        FUN_002249c0(param_6,piVar11);
        param_6 = 0;
        iVar5 = local_5c + 0xc;
        local_70[0] = 0;
        FUN_0028870c(local_70,*(undefined4 *)(local_5c + 0xc));
        FUN_0028870c(iVar5,piVar11);
        FUN_002259fc(piVar11);
        local_48 = 1;
        FUN_0028870c(iVar5,local_70[0]);
        FUN_00288780(local_70);
      }
      goto LAB_00222a1c;
    }
  }
  else {
LAB_0022293c:
    if (uVar12 != 0) goto LAB_00222944;
    puVar7 = (undefined4 *)FUN_001b4b34(DAT_0034d62c,local_58);
    piVar11 = (int *)0x0;
    if (puVar7 != &DAT_003982ec) {
      piVar11 = (int *)puVar7[2];
    }
    if (piVar11 == (int *)0x0 && param_4 != 0) {
      puVar7 = (undefined4 *)FUN_001b4b34(DAT_0034d62c,param_4);
      piVar11 = (int *)0x0;
      if (puVar7 != &DAT_003982ec) {
        piVar11 = (int *)puVar7[2];
      }
    }
    if (piVar11 == (int *)0x0) {
      piVar11 = (int *)FUN_00223828(local_58,0);
      if (((piVar11 == (int *)0x0) || (iVar5 = (**(code **)(*piVar11 + 0x14))(piVar11), iVar5 != 0))
         && (param_4 != 0)) {
        piVar11 = (int *)FUN_00223828(param_4,0);
      }
      if (piVar11 == (int *)0x0) {
        piVar11 = (int *)0x0;
      }
      else {
        iVar5 = (**(code **)(*piVar11 + 0x14))(piVar11);
        if (iVar5 == 0) {
          if (param_8 != (int *)0x0) {
            (**(code **)(*param_8 + 8))(param_8,DAT_0034d614);
            (**(code **)(*(int *)piVar11[10] + 8))((int *)piVar11[10],piVar11[0xb]);
          }
        }
        else {
          piVar11 = (int *)0x0;
        }
      }
    }
    else {
      local_48 = 1;
    }
    if (piVar11 == (int *)0x0) goto LAB_00222944;
  }
  FUN_00224e3c(piVar11,piStackX_0);
  if ((local_50 == 0) && (param_6 != 0)) {
    FUN_002249c0(param_6,piVar11);
    param_6 = 0;
  }
  if (((local_44 != 0) && (piStackX_0 != (int *)0x0)) && (iVar5 = FUN_00223814(piVar11), iVar5 != 0)
     ) {
    sVar1 = *(short *)((int)piVar11 + 0x3e);
    iVar5 = 0;
    if (0 < sVar1) {
      iVar6 = piVar11[0x10];
      while( true ) {
        iVar6 = *(int *)(iVar6 + iVar5 * 4);
        iVar5 = iVar5 + 1;
        if ((*(int *)(iVar6 + 0xc) != 0) ||
           ((**(code **)(*piStackX_0 + 0xc))(piStackX_0,piVar11,iVar6), sVar1 <= iVar5)) break;
        iVar6 = piVar11[0x10];
      }
    }
  }
LAB_00222a1c:
  if (local_50 == 0) {
    if (param_6 != 0) {
      FUN_002249c0(param_6,piVar11);
    }
  }
  else {
    FUN_00225684(*(undefined4 *)(local_5c + 0xc),piVar11);
  }
  if (local_48 != 0) {
    if ((local_44 != 0) && (sVar1 = *(short *)((int)piVar11 + 0x3e), 0 < sVar1)) {
      iVar5 = 1;
      do {
        *(short *)((int)piVar11 + 0x3e) = (short)iVar5;
        FUN_00224e70(piVar11,0);
        bVar2 = iVar5 < sVar1;
        iVar5 = iVar5 + 1;
      } while (bVar2);
    }
    FUN_00224e70(piVar11,1);
  }
  return local_48;
}


// ===== 0020e0bc FUN_0020e0bc (depth 1)

void FUN_0020e0bc(int param_1,int param_2,undefined1 param_3)

{
  uint uVar1;
  uint uVar2;
  uint *puVar3;
  uint uVar4;
  
  uVar4 = 0;
  puVar3 = *(uint **)(param_1 + 8);
  uVar1 = (int)puVar3 + 0x17;
  if (*puVar3 != 0) {
    do {
      uVar2 = uVar1 & 0xfffffff0;
      FUN_001b8e00(uVar2,puVar3,1,param_3);
      *(undefined1 *)(uVar2 + 0x1c) = 0;
      uVar4 = uVar4 + 1;
      uVar1 = uVar2 + 0x4b;
      *(undefined1 *)(uVar2 + 0x1d) = 0;
      *(undefined1 *)(uVar2 + 0x1e) = 0;
      *(undefined1 *)(uVar2 + 0x1f) = 0;
      *(undefined4 *)(uVar2 + *(int *)(param_2 + 0x18)) = *(undefined4 *)(param_2 + 0x10);
      *(uint *)(param_2 + 0x10) = uVar2;
      *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + 1;
    } while (uVar4 < *puVar3);
  }
  return;
}


// ===== 001b9c88 FUN_001b9c88 (depth 1)

undefined4 * FUN_001b9c88(int param_1,undefined4 param_2,undefined4 param_3)

{
  undefined4 *puVar1;
  
  puVar1 = (undefined4 *)FUN_001b9c18();
  if (puVar1 == (undefined4 *)0x0) {
    puVar1 = (undefined4 *)FUN_001baf40(0x20);
    *puVar1 = 0;
    puVar1[1] = param_2;
    puVar1[2] = 0;
    puVar1[3] = 0;
    puVar1[4] = 0;
    puVar1[5] = 0;
    puVar1[6] = 0x18;
    puVar1[7] = param_3;
    *(undefined4 *)((int)puVar1 + *(int *)(param_1 + 0xc)) = *(undefined4 *)(param_1 + 4);
    *(undefined4 **)(param_1 + 4) = puVar1;
    *(int *)(param_1 + 8) = *(int *)(param_1 + 8) + 1;
  }
  return puVar1;
}


// ===== 0020a974 FUN_0020a974 (depth 1)

void FUN_0020a974(int param_1,int param_2,undefined1 param_3)

{
  uint uVar1;
  uint uVar2;
  uint *puVar3;
  uint uVar4;
  
  uVar4 = 0;
  puVar3 = *(uint **)(param_1 + 8);
  uVar1 = (int)puVar3 + 0x17;
  if (*puVar3 != 0) {
    do {
      uVar2 = uVar1 & 0xfffffff0;
      FUN_001b8e00(uVar2,puVar3,1,param_3);
      *(undefined1 *)(uVar2 + 0x1c) = 0;
      uVar4 = uVar4 + 1;
      uVar1 = uVar2 + 0x3f;
      *(undefined1 *)(uVar2 + 0x1d) = 0;
      *(undefined1 *)(uVar2 + 0x1e) = 0;
      *(undefined1 *)(uVar2 + 0x1f) = 0;
      *(undefined4 *)(uVar2 + *(int *)(param_2 + 0x18)) = *(undefined4 *)(param_2 + 0x10);
      *(uint *)(param_2 + 0x10) = uVar2;
      *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + 1;
    } while (uVar4 < *puVar3);
  }
  return;
}


// ===== 0020e214 FUN_0020e214 (depth 1)

void FUN_0020e214(int param_1,int param_2,char param_3)

{
  uint uVar1;
  int iVar2;
  uint uVar3;
  uint *puVar4;
  uint uVar5;
  
  uVar5 = 0;
  puVar4 = *(uint **)(param_1 + 8);
  uVar1 = (int)puVar4 + 0x17;
  if (*puVar4 != 0) {
    do {
      uVar3 = uVar1 & 0xfffffff0;
      FUN_001b8e00(uVar3,puVar4,1,param_3);
      uVar5 = uVar5 + 1;
      uVar1 = uVar3 + 0x4b;
      if ((param_3 == '\0') &&
         (iVar2 = (int)puVar4 + *(int *)(uVar3 + 0x30), *(int *)(uVar3 + 0x30) != 0)) {
        *(char *)(uVar3 + 0x31) = (char)((uint)iVar2 >> 8);
        *(char *)(uVar3 + 0x32) = (char)((uint)iVar2 >> 0x10);
        *(char *)(uVar3 + 0x33) = (char)((uint)iVar2 >> 0x18);
        *(char *)(uVar3 + 0x30) = (char)iVar2;
      }
      *(undefined1 *)(uVar3 + 0x1c) = 0;
      *(undefined1 *)(uVar3 + 0x1d) = 0;
      *(undefined1 *)(uVar3 + 0x1e) = 0;
      *(undefined1 *)(uVar3 + 0x1f) = 0;
      *(undefined4 *)(uVar3 + *(int *)(param_2 + 0x18)) = *(undefined4 *)(param_2 + 0x10);
      *(uint *)(param_2 + 0x10) = uVar3;
      *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + 1;
    } while (uVar5 < *puVar4);
  }
  return;
}


// ===== 0020deec FUN_0020deec (depth 1)

void FUN_0020deec(int param_1,int param_2,undefined1 param_3)

{
  uint uVar1;
  uint uVar2;
  uint *puVar3;
  uint uVar4;
  
  uVar4 = 0;
  puVar3 = *(uint **)(param_1 + 8);
  uVar1 = (int)puVar3 + 0x17;
  if (*puVar3 != 0) {
    do {
      uVar2 = uVar1 & 0xfffffff0;
      FUN_0020e044(uVar2,puVar3,1,param_3);
      *(undefined1 *)(uVar2 + 0x1c) = 0;
      uVar4 = uVar4 + 1;
      uVar1 = uVar2 + 0x47;
      *(undefined1 *)(uVar2 + 0x1d) = 0;
      *(undefined1 *)(uVar2 + 0x1e) = 0;
      *(undefined1 *)(uVar2 + 0x1f) = 0;
      *(undefined4 *)(uVar2 + *(int *)(param_2 + 0x18)) = *(undefined4 *)(param_2 + 0x10);
      *(uint *)(param_2 + 0x10) = uVar2;
      *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + 1;
    } while (uVar4 < *puVar3);
  }
  return;
}


// ===== 0020aacc FUN_0020aacc (depth 1)

void FUN_0020aacc(int param_1,int param_2,undefined1 param_3)

{
  uint uVar1;
  uint uVar2;
  uint *puVar3;
  uint uVar4;
  
  uVar4 = 0;
  puVar3 = *(uint **)(param_1 + 8);
  uVar1 = (int)puVar3 + 0x17;
  if (*puVar3 != 0) {
    do {
      uVar2 = uVar1 & 0xfffffff0;
      FUN_001b8e00(uVar2,puVar3,1,param_3);
      *(undefined1 *)(uVar2 + 0x1c) = 0;
      uVar4 = uVar4 + 1;
      uVar1 = uVar2 + 0x4f;
      *(undefined1 *)(uVar2 + 0x1d) = 0;
      *(undefined1 *)(uVar2 + 0x1e) = 0;
      *(undefined1 *)(uVar2 + 0x1f) = 0;
      *(undefined4 *)(uVar2 + *(int *)(param_2 + 0x18)) = *(undefined4 *)(param_2 + 0x10);
      *(uint *)(param_2 + 0x10) = uVar2;
      *(int *)(param_2 + 0x14) = *(int *)(param_2 + 0x14) + 1;
    } while (uVar4 < *puVar3);
  }
  return;
}


// ===== 002bc0c0 FUN_002bc0c0 (depth 1)

void FUN_002bc0c0(int *param_1,int param_2,int param_3)

{
  int iVar1;
  
  iVar1 = *(int *)(param_2 + param_1[2]);
  if (param_2 == *param_1) {
    *param_1 = iVar1;
  }
  else {
    *(int *)(param_3 + param_1[2]) = iVar1;
  }
  param_1[1] = param_1[1] + -1;
  return;
}


// ===== 0020dfc8 FUN_0020dfc8 (depth 1)

void FUN_0020dfc8(int param_1)

{
  int iVar1;
  int iVar2;
  undefined4 local_1c;
  
  local_1c = *(int *)(param_1 + 0x10);
  if (local_1c != 0) {
    do {
      iVar1 = local_1c;
      FUN_001b9100(local_1c);
      iVar2 = *(int *)(local_1c + *(int *)(param_1 + 0x18));
      if (iVar2 != 0) {
        local_1c = iVar2;
      }
    } while (iVar1 != local_1c);
  }
  return;
}


// ===== 0020e198 FUN_0020e198 (depth 1)

void FUN_0020e198(int param_1)

{
  int iVar1;
  int iVar2;
  undefined4 local_1c;
  
  local_1c = *(int *)(param_1 + 0x10);
  if (local_1c != 0) {
    do {
      iVar1 = local_1c;
      FUN_001b9100(local_1c);
      iVar2 = *(int *)(local_1c + *(int *)(param_1 + 0x18));
      if (iVar2 != 0) {
        local_1c = iVar2;
      }
    } while (iVar1 != local_1c);
  }
  return;
}


// ===== 0020aa50 FUN_0020aa50 (depth 1)

void FUN_0020aa50(int param_1)

{
  int iVar1;
  int iVar2;
  undefined4 local_1c;
  
  local_1c = *(int *)(param_1 + 0x10);
  if (local_1c != 0) {
    do {
      iVar1 = local_1c;
      FUN_001b9100(local_1c);
      iVar2 = *(int *)(local_1c + *(int *)(param_1 + 0x18));
      if (iVar2 != 0) {
        local_1c = iVar2;
      }
    } while (iVar1 != local_1c);
  }
  return;
}


// ===== 001bb13c FUN_001bb13c (depth 1)

void FUN_001bb13c(int param_1)

{
  if (param_1 != 0) {
    DAT_003985b8 = DAT_003985b8 + 1;
    FUN_001bbf84(&DAT_00398560,param_1);
  }
  return;
}


// ===== 001bb178 FUN_001bb178 (depth 1)

void FUN_001bb178(void)

{
  FUN_001bb13c();
  return;
}


// ===== 00222430 FUN_00222430 (depth 1)

undefined4 * FUN_00222430(void)

{
  return &DAT_003f18f8;
}


// ===== 0028870c FUN_0028870c (depth 1)

int * FUN_0028870c(int *param_1,int *param_2)

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


// ===== 002259fc FUN_002259fc (depth 1)

void FUN_002259fc(int *param_1)

{
  byte bVar1;
  short sVar2;
  int *piVar3;
  int iVar4;
  ushort uVar5;
  int *piVar6;
  uint uVar7;
  int iVar8;
  undefined4 uVar9;
  undefined4 *puVar10;
  undefined2 uVar11;
  int local_20 [4];
  
  iVar8 = param_1[6];
  if (iVar8 == 0) {
    iVar8 = FUN_00221878(&DAT_0034cc40,param_1[0xe],local_20,(uint)param_1[0x11] >> 1 & 1);
    param_1[6] = iVar8;
    if ((iVar8 == 0) || (local_20[0] == 0)) {
      param_1[3] = 2;
      return;
    }
    piVar3 = (int *)FUN_001baf74(local_20[0] * 4 + 4);
    *piVar3 = local_20[0];
    iVar8 = local_20[0] + -1;
    piVar6 = piVar3 + 1;
    if (local_20[0] != 0) {
      do {
        iVar8 = iVar8 + -1;
        *piVar6 = 0;
        piVar6 = piVar6 + 1;
      } while (iVar8 != -1);
    }
    iVar8 = param_1[6];
    param_1[0x10] = (int)(piVar3 + 1);
  }
LAB_00225a1c:
  iVar4 = (int)(short)param_1[0xf];
LAB_00225a20:
  while( true ) {
    *(short *)(param_1 + 0xf) = (short)iVar4 + 1;
    puVar10 = (undefined4 *)(iVar8 + iVar4 * 0x18);
    uVar7 = puVar10[4];
    if (uVar7 == 0xfffffe) {
      param_1[3] = 0;
      (**(code **)(*param_1 + 0x28))(param_1);
      FUN_00223a58(param_1);
      return;
    }
    uVar5 = *(ushort *)(puVar10 + 3) & 0xf;
    if (uVar5 == 5) goto LAB_00225a1c;
    if (uVar5 != 6) break;
    iVar4 = (int)(short)param_1[0xf];
  }
  if (((uint)param_1[0x11] >> 1 & 1) == 0) {
    iVar8 = puVar10[2];
  }
  else {
    if ((uVar5 == 2) || (uVar5 == 6)) {
      if ((uVar7 >> 0x18 & 1) == 0) goto code_r0x00225bd0;
      iVar4 = (int)(short)param_1[0xf];
      goto LAB_00225a20;
    }
    if ((uVar7 >> 0x18 & 1) == 0) {
      sVar2 = *(short *)((int)param_1 + 0x46);
    }
    else {
      *(short *)((int)param_1 + 0x46) = *(short *)((int)param_1 + 0x46) + -1;
      sVar2 = *(short *)((int)param_1 + 0x46);
    }
    if (sVar2 < 1) {
      iVar8 = FUN_00223828(*puVar10,0);
      if (iVar8 != 0) {
        iVar8 = puVar10[2];
        goto LAB_00225ab4;
      }
      iVar8 = param_1[6];
      goto LAB_00225a1c;
    }
    iVar8 = puVar10[2];
  }
LAB_00225ab4:
  if (iVar8 == 0x4d5350) {
    if ((*(byte *)(param_1 + 0x11) & 1) == 0) {
      iVar8 = FUN_00223828(*puVar10,0);
      if (iVar8 != 0) {
        uVar9 = puVar10[2];
        goto LAB_00225b2c;
      }
      iVar8 = param_1[6];
      goto LAB_00225a1c;
    }
    bVar1 = *(byte *)(param_1 + 0xd);
    uVar9 = 0x4d5350;
  }
  else {
    iVar8 = FUN_00232200(iVar8);
    if (iVar8 == 0) {
      uVar9 = puVar10[2];
    }
    else {
      uVar7 = (uint)puVar10[4] >> 0x19 & 1;
      iVar8 = FUN_0023224c(puVar10[2],(*(ushort *)(puVar10 + 3) & 0xf0) >> 4);
      uVar11 = (undefined2)uVar7;
      if (iVar8 == 0) {
        if ((*(ushort *)(puVar10 + 3) & 0xf0) != 0) {
          iVar8 = param_1[6];
          goto LAB_00225a1c;
        }
        if ((short)param_1[0x12] < 0) {
          *(undefined2 *)(param_1 + 0x12) = uVar11;
        }
        else {
          if ((int)(short)param_1[0x12] == uVar7) {
            iVar8 = param_1[6];
            goto LAB_00225a1c;
          }
          *(undefined2 *)(param_1 + 0x12) = uVar11;
        }
      }
      else {
        *(undefined2 *)(param_1 + 0x12) = uVar11;
      }
      uVar9 = puVar10[2];
    }
LAB_00225b2c:
    bVar1 = *(byte *)(param_1 + 0xd);
  }
  uVar7 = 0xffffffff;
  if (bVar1 != 0xff) {
    uVar7 = (uint)bVar1;
  }
  FUN_002228b4(&DAT_0034d600,"No-name",*puVar10,0,uVar9,0,uVar7,0,0,0,0,1,0);
  iVar8 = param_1[6];
  goto LAB_00225a1c;
code_r0x00225bd0:
  *(short *)((int)param_1 + 0x46) = *(short *)((int)param_1 + 0x46) + 1;
  goto LAB_00225a1c;
}


// ===== 00223190 FUN_00223190 (depth 1)

undefined4 FUN_00223190(undefined4 param_1)

{
  int iVar1;
  
  iVar1 = FUN_001b8ba0(&DAT_003d8bd8,param_1);
  return *(undefined4 *)(iVar1 + 0xc);
}


// ===== 00225684 FUN_00225684 (depth 1)

void FUN_00225684(int param_1,undefined4 param_2)

{
  short sVar1;
  
  sVar1 = *(short *)(param_1 + 0x3e);
  *(short *)(param_1 + 0x3e) = sVar1 + 1;
  FUN_002c3b68(*(int *)(param_1 + 0x40) + sVar1 * 4);
  DAT_003f1924 = param_2;
  return;
}


// ===== 00223f98 FUN_00223f98 (depth 1)

int * FUN_00223f98(int param_1,int param_2)

{
  int iVar1;
  int *piVar2;
  int iVar3;
  int iVar4;
  int iVar5;
  int iVar6;
  
  if (*(int *)(param_1 + 0xc) != 0) {
    iVar5 = *(int *)(param_1 + 0x24);
    iVar6 = 0;
    if (0 < iVar5) {
      do {
        iVar4 = iVar6 + (iVar5 >> 1);
        piVar2 = (int *)(iVar4 * 0x10 + *(int *)(param_1 + 8));
        iVar3 = *piVar2;
        iVar1 = iVar6 - iVar4;
        if (iVar3 == param_2) {
          return piVar2;
        }
        if (param_2 < iVar3) {
          iVar5 = iVar4 - iVar6;
        }
        else {
          iVar6 = iVar4 + 1;
          iVar5 = iVar5 + iVar1 + -1;
        }
      } while (0 < iVar5);
    }
  }
  return (int *)0x0;
}


// ===== 00223828 FUN_00223828 (depth 1)

void FUN_00223828(undefined4 param_1,undefined1 param_2)

{
  undefined1 auStack_30 [32];
  
  FUN_00223c18(auStack_30,&DAT_0034d600,param_1);
  FUN_00223c38(auStack_30,0,param_2);
  return;
}


// ===== 00224e70 FUN_00224e70 (depth 1)

void FUN_00224e70(int *param_1,char param_2)

{
  int *piVar1;
  int iVar2;
  char cVar3;
  int *local_2c;
  int *local_28;
  
  iVar2 = DAT_003f1920 * 4;
  DAT_003f1920 = DAT_003f1920 + 1;
  *(int **)(&DAT_003f1900 + iVar2) = param_1;
  local_2c = (int *)param_1[7];
  if ((local_2c != (int *)0x0) && (local_28 = (int *)0x0, local_2c != (int *)0x0)) {
    do {
      piVar1 = local_2c;
      if ((param_2 != '\0') &&
         (local_2c = (int *)FUN_002c3af8(param_1 + 7,local_2c,local_28), local_2c == (int *)0x0)) {
        local_2c = local_28;
      }
      if (piVar1[1] == 0) {
        if ((param_2 == '\0') &&
           (local_2c = (int *)FUN_002c3af8(param_1 + 7,local_2c,local_28), local_2c == (int *)0x0))
        {
          local_2c = local_28;
        }
        cVar3 = '\x01';
        if (piVar1 != (int *)0x0) {
          (**(code **)(*piVar1 + 4))(piVar1);
        }
      }
      else {
        if (param_2 != '\0') {
          *(undefined1 *)(piVar1 + 5) = 1;
        }
        iVar2 = piVar1[4];
        (**(code **)(*param_1 + 0x24))(param_1,piVar1,param_2);
        cVar3 = param_2;
        if (((iVar2 != 0) && (param_2 != '\0')) && (piVar1[4] != 0)) {
          FUN_002c3a84(piVar1[4],0);
        }
      }
      if (cVar3 == '\0') {
        local_28 = local_2c;
        if (*(int **)((int)local_2c + param_1[9]) != (int *)0x0) {
          local_2c = *(int **)((int)local_2c + param_1[9]);
        }
      }
    } while (local_28 != local_2c);
  }
  DAT_003f1920 = DAT_003f1920 + -1;
  return;
}


// ===== 00225030 FUN_00225030 (depth 1)

void FUN_00225030(undefined4 *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5,undefined4 param_6)

{
  FUN_002249dc(param_1,param_3,param_2,param_5,param_6);
  param_1[0xf] = param_4;
  param_1[0x11] = param_1[0x11] & 0xffffff80;
  *param_1 = &PTR_FUN_0033de88;
  param_1[0x10] = 0;
  return;
}


// ===== 00222fec FUN_00222fec (depth 1)

int * FUN_00222fec(undefined4 param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                  undefined4 param_5,undefined4 param_6,int param_7,int param_8,byte bStackX_0,
                  byte bStackX_4)

{
  undefined4 *puVar1;
  int iVar2;
  undefined4 uVar3;
  int *piVar4;
  
  puVar1 = (undefined4 *)FUN_001b4b34(DAT_0034d62c,param_4);
  piVar4 = (int *)0x0;
  if (puVar1 != &DAT_003982ec) {
    piVar4 = (int *)puVar1[2];
  }
  if (((piVar4 == (int *)0x0) || (iVar2 = (**(code **)(*piVar4 + 0x14))(), iVar2 != 0)) &&
     ((piVar4 = (int *)FUN_00223828(param_4,0), piVar4 == (int *)0x0 ||
      (iVar2 = (**(code **)(*piVar4 + 0x14))(piVar4), iVar2 != 0)))) {
    piVar4 = (int *)FUN_001baf40(0x48);
    uVar3 = FUN_00222430();
    FUN_00225030(piVar4,param_6,param_4,param_5,uVar3,0);
    if (param_7 != -1) {
      piVar4[0xc] = param_7;
    }
    if (param_8 != -1) {
      *(char *)(piVar4 + 0xd) = (char)param_8;
    }
    piVar4[0x11] = piVar4[0x11] & 0xffffff9fU | (bStackX_0 & 1) << 5 | (bStackX_4 & 1) << 6;
    FUN_002253a8(piVar4,param_2,param_3,param_5);
    (**(code **)(*piVar4 + 0x28))(piVar4);
    piVar4[0x11] = piVar4[0x11] & 0xffffffef;
  }
  return piVar4;
}


// ===== 00223814 FUN_00223814 (depth 1)

bool FUN_00223814(int param_1)

{
  return DAT_0034d60c == param_1;
}


// ===== 00225518 FUN_00225518 (depth 1)

void FUN_00225518(undefined4 *param_1,undefined4 param_2,undefined4 param_3,undefined4 param_4,
                 undefined4 param_5,uint param_6)

{
  FUN_002249dc(param_1,param_3,param_2);
  *(undefined2 *)(param_1 + 0x12) = 0xffff;
  param_1[0x11] = param_1[0x11] & 0xfffffffc | (param_6 & 1) << 1;
  *param_1 = &PTR_FUN_0033de50;
  *(undefined2 *)(param_1 + 0xf) = 0;
  *(undefined2 *)((int)param_1 + 0x3e) = 0;
  param_1[0x10] = 0;
  *(undefined2 *)((int)param_1 + 0x46) = 0;
  return;
}


// ===== 00221878 FUN_00221878 (depth 1)

undefined4 * FUN_00221878(int param_1,undefined4 param_2,int *param_3,char param_4)

{
  byte bVar1;
  ushort uVar2;
  undefined2 uVar3;
  int iVar4;
  undefined4 uVar5;
  int iVar6;
  int iVar7;
  undefined4 *puVar8;
  ushort uVar9;
  ushort uVar10;
  undefined4 *puVar11;
  int *piVar12;
  int iVar13;
  undefined4 uVar14;
  undefined4 uVar15;
  int iVar16;
  undefined4 *puVar17;
  undefined4 *puVar18;
  int iVar19;
  undefined4 *puVar20;
  int local_30 [4];
  
  *param_3 = 0;
  uVar5 = FUN_002209c4(&DAT_0034d488,param_2,local_30,0);
  if (local_30[0] == 0) {
    return (undefined4 *)0x0;
  }
  iVar6 = FUN_00221cf4(param_1,uVar5);
  uVar2 = *(ushort *)(iVar6 + 0x14);
  iVar19 = *(int *)(param_1 + 0x84c) + (uint)uVar2 * 0x14;
  iVar7 = FUN_002207e0(*(undefined4 *)(iVar19 + 8),param_2);
  iVar16 = *(int *)(iVar19 + 8);
  piVar12 = (int *)(*(int *)(iVar16 + 4) + (uint)*(ushort *)(iVar7 + 0xe) * 8);
  if (*piVar12 != *(int *)(iVar6 + 0x10)) {
    uVar10 = *(ushort *)((int)piVar12 + 6);
    while (piVar12 = (int *)(*(int *)(iVar16 + 4) + (uint)uVar10 * 8),
          *piVar12 != *(int *)(iVar6 + 0x10)) {
      uVar10 = *(ushort *)((int)piVar12 + 6);
    }
  }
  puVar20 = &DAT_003d8c40;
  piVar12 = piVar12 + 2;
  iVar7 = 0;
  iVar6 = 0;
  do {
    iVar13 = *(int *)(iVar16 + 8) + (uint)*(ushort *)(piVar12 + 1) * 0x14;
    uVar10 = *(ushort *)(iVar13 + 0xc);
    uVar9 = uVar10 & 0xf;
    if ((((uVar9 == 2) || (uVar9 == 6)) || (uVar9 == 3)) &&
       (((uVar10 & 0xf) == 5 || ((uVar10 & 0xf) == 6)))) {
      iVar4 = *piVar12;
      iVar13 = iVar6 + 1;
      piVar12 = piVar12 + 2;
      if (iVar6 != iVar13) {
        do {
          uVar10 = *(ushort *)(*(int *)(iVar16 + 8) + (uint)*(ushort *)(piVar12 + 1) * 0x14 + 0xc) &
                   0xf;
          if (((uVar10 == 2) || (uVar10 == 6)) || (uVar10 == 3)) {
            iVar13 = iVar13 + 1;
            bVar1 = *(byte *)((int)piVar12 + 3);
          }
          else {
            bVar1 = *(byte *)((int)piVar12 + 3);
          }
          iVar13 = iVar13 - (bVar1 & 1);
          piVar12 = piVar12 + 2;
        } while (iVar6 != iVar13);
      }
      if ((iVar4 & 0x1000000) != 0) {
        iVar13 = iVar13 + -1;
      }
    }
    else {
      uVar9 = uVar10 & 0xf;
      if ((uVar9 == 2) || ((uVar9 == 6 || (uVar9 == 3)))) {
        iVar6 = iVar6 + 1;
        iVar7 = iVar7 + 1;
      }
      if (param_4 == '\0') {
        uVar10 = uVar10 & 0xf;
        if ((uVar10 == 2) || (uVar10 == 6)) goto LAB_00221a04;
        if (uVar10 != 3) {
          puVar20[1] = iVar13;
          goto LAB_002219fc;
        }
        bVar1 = *(byte *)((int)piVar12 + 3);
      }
      else {
        puVar20[1] = iVar13;
LAB_002219fc:
        *puVar20 = piVar12;
        puVar20 = puVar20 + 2;
LAB_00221a04:
        bVar1 = *(byte *)((int)piVar12 + 3);
      }
      piVar12 = piVar12 + 2;
      iVar13 = iVar6 - (bVar1 & 1);
    }
    if (iVar13 < 0) {
      iVar6 = (int)(puVar20 + -0xf6310) >> 3;
      *param_3 = iVar6;
      puVar20 = (undefined4 *)FUN_001baf74((iVar6 + 1) * 0x18);
      if (iVar6 == -1) {
        iVar6 = *param_3;
      }
      else {
        do {
          iVar6 = iVar6 + -1;
        } while (iVar6 != -1);
        iVar6 = *param_3;
      }
      iVar16 = iVar6 + -1;
      if (-1 < iVar16) {
        puVar18 = &DAT_003d8c40 + iVar16 * 2;
        puVar17 = puVar20 + iVar16 * 6;
        do {
          puVar11 = (undefined4 *)*puVar18;
          puVar8 = (undefined4 *)puVar18[1];
          iVar16 = iVar16 + -1;
          puVar18 = puVar18 + -2;
          uVar5 = *puVar8;
          uVar3 = *(undefined2 *)(puVar8 + 3);
          uVar14 = puVar8[1];
          uVar15 = puVar8[2];
          puVar17[4] = *puVar11;
          *puVar17 = uVar5;
          puVar17[1] = uVar14;
          puVar17[2] = uVar15;
          *(undefined2 *)(puVar17 + 3) = uVar3;
          *(ushort *)(puVar17 + 5) = uVar2;
          puVar17 = puVar17 + -6;
        } while (-1 < iVar16);
        iVar6 = *param_3;
      }
      puVar17 = puVar20 + iVar6 * 6;
      puVar17[4] = puVar20[4];
      uVar5 = puVar20[1];
      uVar14 = puVar20[2];
      uVar3 = *(undefined2 *)(puVar20 + 3);
      *puVar17 = *puVar20;
      puVar17[1] = uVar5;
      puVar17[2] = uVar14;
      *(undefined2 *)(puVar17 + 3) = uVar3;
      *(undefined2 *)(puVar17 + 5) = *(undefined2 *)(puVar20 + 5);
      puVar20[*param_3 * 6 + 4] = 0xfffffe;
      if (param_4 == '\0') {
        return puVar20;
      }
      *param_3 = *param_3 - iVar7;
      return puVar20;
    }
    iVar16 = *(int *)(iVar19 + 8);
    iVar6 = iVar13;
  } while( true );
}


// ===== 00224e3c FUN_00224e3c (depth 1)

void FUN_00224e3c(int param_1,int param_2)

{
  if (param_2 != 0) {
    *(undefined4 *)(param_2 + *(int *)(param_1 + 0x24)) = *(undefined4 *)(param_1 + 0x1c);
    *(int *)(param_1 + 0x1c) = param_2;
    *(int *)(param_1 + 0x20) = *(int *)(param_1 + 0x20) + 1;
  }
  return;
}


// ===== 001b4b34 FUN_001b4b34 (depth 1)

undefined4 * FUN_001b4b34(undefined4 *param_1,int param_2)

{
  int iVar1;
  
  if ((param_1 != &DAT_003982ec) && (iVar1 = param_1[1], iVar1 != param_2)) {
    while( true ) {
      if (param_2 < iVar1) {
        param_1 = (undefined4 *)param_1[5];
      }
      else {
        param_1 = (undefined4 *)param_1[6];
      }
      if ((param_1 == &DAT_003982ec) || (param_1[1] == param_2)) break;
      iVar1 = param_1[1];
    }
  }
  return param_1;
}


// ===== 00221558 FUN_00221558 (depth 1)

void FUN_00221558(int param_1)

{
  FUN_00220950(param_1 + 0x848);
  return;
}


// ===== 001baf40 FUN_001baf40 (depth 1)

void FUN_001baf40(undefined4 param_1)

{
  DAT_003985b4 = DAT_003985b4 + 1;
  FUN_001bbd88(&DAT_00398560,param_1);
  return;
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


// ===== 002249c0 FUN_002249c0 (depth 1)

void FUN_002249c0(void)

{
  FUN_0028870c();
  return;
}


// ===== 00288780 FUN_00288780 (depth 1)

void FUN_00288780(int *param_1)

{
  param_1 = (int *)*param_1;
  if (param_1 != (int *)0x0) {
    (**(code **)(*param_1 + 8))(param_1);
  }
  return;
}


