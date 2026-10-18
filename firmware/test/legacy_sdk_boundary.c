#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../lib/BoschBMI270/src/bmi270_legacy.c"
static uint8_t pages[8][16];
static uint8_t selected_page;
static BMI2_INTF_RETURN_TYPE read_fake(uint8_t reg, uint8_t *buf, uint32_t len, void *ptr) {
 (void)ptr;
 if (reg >= 0x30 && reg + len <= 0x40) memcpy(buf, pages[selected_page] + (reg-0x30), len);
 else memset(buf,0,len);
 return BMI2_INTF_RET_SUCCESS;
}
static BMI2_INTF_RETURN_TYPE write_fake(uint8_t reg, const uint8_t *buf, uint32_t len, void *ptr) {
 (void)ptr;
 if (reg==0x2f && len==1) selected_page=*buf;
 else if (reg>=0x30 && reg + len <= 0x40) memcpy(pages[selected_page]+(reg-0x30),buf,len);
 return BMI2_INTF_RET_SUCCESS;
}
static void delay_fake(uint32_t us, void *ptr) { (void)us; (void)ptr; }
int main(void) {
 for (int p=0;p<8;p++) for (int i=0;i<16;i++) pages[p][i]=(uint8_t)(p*16+i);
 uint8_t expected[8][16]; memcpy(expected,pages,sizeof(pages));
 expected[5][0] &= ~8; expected[5][4]=9; expected[5][5]=0; expected[5][6]=80; expected[5][7]=0; expected[5][14]=80; expected[5][15]=0;
 expected[6][0]=1; expected[6][1]=0; expected[6][4]=(expected[6][4]&~3)|2;
 struct bmi2_dev dev={0};
 struct bmi2_tap_config tap={0};
 dev.intf=BMI2_I2C_INTF;
 dev.read=read_fake; dev.write=write_fake; dev.delay_us=delay_fake;
 dev.read_write_len=16; dev.page_max=8;
 dev.feat_config=bmi270_legacy_feat_in; dev.input_sens=BMI270_LEGACY_MAX_FEAT_IN;
 tap.data_reg_en=0; tap.tap_sens_thres=9; tap.max_gest_dur=80; tap.quite_time_after_gest=80;
 tap.wait_for_timeout=1; tap.axis_sel=2;
 int rslt=set_tap_config(&tap,&dev);
 if (memcmp(expected,pages,sizeof(pages))) { puts("Unexpected page writes"); return 2; }
 // Exercise the actual SDK getters/setters for every diagnostic combination;
 // preserve all bytes outside the selected tap fields and only enable double.
 unsigned cases=0;
 const uint16_t sensitivities[]={0,3,9,15};
 for(unsigned source=0;source<2;++source)for(unsigned axis=0;axis<3;++axis)for(unsigned s=0;s<4;++s){
  memcpy(expected,pages,sizeof(pages));
  expected[5][0]=(expected[5][0]&~8)|(source<<3);
  expected[5][4]=sensitivities[s];expected[5][5]=0;
  expected[6][4]=(expected[6][4]&~3)|axis;
  tap.data_reg_en=source;tap.axis_sel=axis;tap.tap_sens_thres=sensitivities[s];
  assert(set_tap_config(&tap,&dev)==BMI2_OK);
  assert(memcmp(expected,pages,sizeof(pages))==0);
  struct bmi2_tap_config back={0};assert(get_tap_config(&back,&dev)==BMI2_OK);
  assert(back.data_reg_en==source&&back.axis_sel==axis&&back.tap_sens_thres==sensitivities[s]);
  assert(back.max_gest_dur==80&&back.quite_time_after_gest==80&&back.wait_for_timeout==1);
  uint8_t all[]={BMI2_SINGLE_TAP,BMI2_DOUBLE_TAP,BMI2_TRIPLE_TAP};
  assert(bmi270_legacy_sensor_disable(all,3,&dev)==BMI2_OK);
  assert((pages[5][0]&7)==0);
  assert(bmi270_legacy_sensor_enable(&all[1],1,&dev)==BMI2_OK);
  assert((pages[5][0]&7)==2);
  ++cases;
 }
 printf("%u SDK sensitivity/source/axis combinations, readback and double-only enable passed\n",cases);
 // Real SDK headerless FIFO parsing must preserve signed raw sensor axes.
 uint8_t bytes[]={0x00,0x10,0x00,0xe0,0x00,0x08,0xff,0x7f,0x00,0x80,0xff,0xff};
 struct bmi2_fifo_frame fifo={0};fifo.data=bytes;fifo.length=sizeof(bytes);fifo.data_enable=BMI2_FIFO_ACC_EN;
 fifo.acc_frm_len=6;dev.remap.x_axis=0;dev.remap.y_axis=1;dev.remap.z_axis=2;
 dev.remap.x_axis_sign=dev.remap.y_axis_sign=dev.remap.z_axis_sign=BMI2_POS_SIGN;
 struct bmi2_sens_axes_data samples[2]={{0}};uint16_t count=2;
 assert(bmi2_extract_accel(samples,&count,&fifo,&dev)>=0&&count==2);
 assert(samples[0].x==4096&&samples[0].y==-8192&&samples[0].z==2048);
 assert(samples[1].x==32767&&samples[1].y==-32768&&samples[1].z==-1);
 puts("SDK accel-only headerless FIFO extraction and signed extrema passed");
 printf("result=%d page=%d bytes=%u dev_size=%zu\n",rslt,selected_page,BMI2_FEAT_SIZE_IN_BYTES,sizeof(dev));
 return rslt != BMI2_OK;
}
