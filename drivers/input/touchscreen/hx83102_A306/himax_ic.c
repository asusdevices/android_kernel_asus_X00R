/* Himax Android Driver Sample Code for HX83102 chipset
*
* Copyright (C) 2017 Himax Corporation.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

#include "himax_ic.h"

extern int hx83102_i2c_error_count;

extern unsigned long	HX83102_FW_VER_MAJ_FLASH_ADDR;
extern unsigned long 	HX83102_FW_VER_MIN_FLASH_ADDR;
extern unsigned long 	HX83102_CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long 	HX83102_CFG_VER_MIN_FLASH_ADDR;
extern unsigned long 	HX83102_CID_VER_MAJ_FLASH_ADDR;
extern unsigned long 	HX83102_CID_VER_MIN_FLASH_ADDR;

extern unsigned long 	HX83102_FW_VER_MAJ_FLASH_LENG;
extern unsigned long 	HX83102_FW_VER_MIN_FLASH_LENG;
extern unsigned long 	HX83102_CFG_VER_MAJ_FLASH_LENG;
extern unsigned long 	HX83102_CFG_VER_MIN_FLASH_LENG;
extern unsigned long 	HX83102_CID_VER_MAJ_FLASH_LENG;
extern unsigned long 	HX83102_CID_VER_MIN_FLASH_LENG;

#ifdef HX_AUTO_UPDATE_FW
extern int g_hx83102_i_FW_VER;
extern int g_hx83102_i_CFG_VER;
extern int g_hx83102_i_CID_MAJ;
extern int g_hx83102_i_CID_MIN;
extern unsigned char hx83102_i_CTPM_FW[];
#endif

extern unsigned char	hx83102_IC_TYPE;
extern unsigned char	hx83102_IC_CHECKSUM;

extern bool hx83102_DSRAM_Flag;

extern struct hx_83102_ic_data* hx83102_ic_data;
extern struct hx_83102_ts_data *hx83102_ts;

static int himax_touch_data_size = 128;

#ifdef HX_TP_PROC_2T2R
bool hx83102_Is_2T2R = false;
#endif

#ifdef HX_USB_DETECT_GLOBAL
//extern kal_bool upmu_is_chr_det(void);
extern void hx83102_cable_detect_func(bool force_renew);
#endif

int hx83102_get_touch_data_size(void)
{
    return himax_touch_data_size;
}

#ifdef HX_RST_PIN_FUNC
extern void hx83102_rst_gpio_set(int pinnum, uint8_t value);
extern int hx83102_report_data_init(void);
extern u8 HX83102_HW_RESET_ACTIVATE;

static void himax_pin_reset(void)
{
	I("%s: Now reset the Touch chip.\n", __func__);
	hx83102_rst_gpio_set(hx83102_ts->rst_gpio, 0);
	msleep(20);
	hx83102_rst_gpio_set(hx83102_ts->rst_gpio, 1);
	msleep(50);
}

static void himax_reload_config(void)
{
    if(hx83102_report_data_init())
        E("%s: allocate data fail\n",__func__);
    
    hx83102_sense_on(hx83102_ts->client, 0x00);
}

static void himax_irq_switch(int switch_on)
{
    int ret = 0;
    if(switch_on)
    {

        if (hx83102_ts->use_irq)
            hx83102_int_enable(hx83102_ts->client->irq,switch_on);
        else
            hrtimer_start(&hx83102_ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
    }
    else
    {
        if (hx83102_ts->use_irq)
            hx83102_int_enable(hx83102_ts->client->irq,switch_on);
        else
        {
            hrtimer_cancel(&hx83102_ts->timer);
            ret = cancel_work_sync(&hx83102_ts->work);
        }
    }
}

void hx83102_ic_reset(uint8_t loadconfig,uint8_t int_off)
{
    struct hx_83102_ts_data *ts = hx83102_ts;

    HX83102_HW_RESET_ACTIVATE = 1;

    I("%s,status: loadconfig=%d,int_off=%d\n",__func__,loadconfig,int_off);

    if (ts->rst_gpio)
    {
        if(int_off)
        {
            himax_irq_switch(0);
        }

        himax_pin_reset();
        if(loadconfig)
        {
            himax_reload_config();
        }
        if(int_off)
        {
            himax_irq_switch(1);
        }
    }

}
#endif

#if defined(HX_ESD_RECOVERY)
int g_hx83102_zero_event_count = 0;
int hx83102_ic_esd_recovery(int hx_esd_event,int hx_zero_event,int length)
{
    if(g_hx83102_zero_event_count > 5)
    {
        g_hx83102_zero_event_count = 0;
        I("[HIMAX TP MSG]: ESD event checked - ALL Zero.\n");
        goto checksum_fail;
    }

    if(hx_esd_event == length)
    {
    	g_hx83102_zero_event_count = 0;
        goto checksum_fail;
    }
    else if(hx_zero_event == length)
    {
        g_hx83102_zero_event_count++;
        I("[HIMAX TP MSG]: ALL Zero event is %d times.\n",g_hx83102_zero_event_count);
        goto err_workqueue_out;
    }

checksum_fail:
    return CHECKSUM_FAIL;
err_workqueue_out:
    return WORK_OUT;
}
void hx83102_esd_ic_reset(void)
{
	HX83102_ESD_RESET_ACTIVATE = 1;
#ifdef HX_RST_PIN_FUNC
	himax_pin_reset();
#endif
	E("%s:\n", __func__);
}
#endif

static int himax_flash_write_burst(struct i2c_client *client, uint8_t * reg_byte, uint8_t * write_data)
{
    uint8_t data_byte[8];
    int i = 0, j = 0;

    for (i = 0; i < 4; i++)
    {
        data_byte[i] = reg_byte[i];
    }
    for (j = 4; j < 8; j++)
    {
        data_byte[j] = write_data[j-4];
    }

    if ( hx83102_i2c_himax_write(client, 0x00,data_byte, 8, DEFAULT_RETRY_CNT) < 0)
    {
        E("%s: i2c access fail!\n", __func__);
        return I2C_FAIL;
    }
	return 0;

}

static void himax_flash_write_burst_lenth(struct i2c_client *client, uint8_t *reg_byte, uint8_t *write_data, int length)
{
    uint8_t data_byte[256];
    int i = 0, j = 0;

    for (i = 0; i < 4; i++)
    {
        data_byte[i] = reg_byte[i];
    }
    for (j = 4; j < length + 4; j++)
    {
        data_byte[j] = write_data[j - 4];
    }

    if ( hx83102_i2c_himax_write(client, 0x00,data_byte, length + 4, DEFAULT_RETRY_CNT) < 0)
    {
        E("%s: i2c access fail!\n", __func__);
        return;
    }
}

static void himax_interface_on(struct i2c_client *client)
{
    uint8_t tmp_data[5];
    uint8_t tmp_data2[2];
    int cnt = 0;

    //Read a dummy register to wake up I2C.
    if ( hx83102_i2c_himax_read(client, 0x08, tmp_data,4,DEFAULT_RETRY_CNT) < 0)  // to knock I2C
    {
        E("%s: i2c access fail!\n", __func__);
        return;
    }

    do
    {
        //===========================================
        // Enable continuous burst mode : 0x13 ==> 0x31
        //===========================================
        tmp_data[0] = 0x31;
        if ( hx83102_i2c_himax_write(client, 0x13,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return;
        }
        //===========================================
        // AHB address auto +4		: 0x0D ==> 0x11
        // Do not AHB address auto +4 : 0x0D ==> 0x10
        //===========================================
        tmp_data[0] = (0x10);
        if ( hx83102_i2c_himax_write(client, 0x0D,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return;
        }

        // Check cmd
        hx83102_i2c_himax_read(client, 0x13, tmp_data,1,DEFAULT_RETRY_CNT);
        hx83102_i2c_himax_read(client, 0x0D, tmp_data2,1,DEFAULT_RETRY_CNT);

        if (tmp_data[0] == 0x31 && tmp_data2[0] == 0x10)
        {
            //isBusrtOn = true;
            break;
        }
        msleep(1);
    }
    while (++cnt < 10);

    if (cnt > 0)
        I("%s:Polling burst mode: %d times", __func__,cnt);

}

int hx83102_hand_shaking(struct i2c_client *client)    //0:Running, 1:Stop, 2:I2C Fail
{
    int result = 0;

    return result;
}

void hx83102_idle_mode(struct i2c_client *client,int disable)
{
    int retry = 20;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t switch_cmd = 0x00;

    I("%s:entering\n",__func__);
    do
    {

        I("%s,now %d times\n!",__func__,retry);

        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x70;
        tmp_addr[0] = 0x88;
        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);

        if(disable)
            switch_cmd = 0x17;
        else
            switch_cmd = 0x1F;

        tmp_data[0] = switch_cmd;
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
        I("%s:After turn ON/OFF IDLE Mode [0] = 0x%02X,[1] = 0x%02X,[2] = 0x%02X,[3] = 0x%02X\n", __func__,tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3]);

        retry--;
        msleep(10);

    }
    while((tmp_data[0] != switch_cmd) && retry > 0);

    I("%s: setting OK!\n",__func__);

}

static int himax_write_read_reg(struct i2c_client *client,uint8_t *tmp_addr,uint8_t *tmp_data,uint8_t hb,uint8_t lb)
{
    int cnt = 0;

    do
    {
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        msleep(10);
        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
        //I("%s:Now tmp_data[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",__func__,tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3]);
    }
    while((tmp_data[1] != hb && tmp_data[0] != lb) && cnt++ < 100);

    if(cnt == 99)
        return -1;

    I("Now register 0x%08X : high byte=0x%02X,low byte=0x%02X\n",tmp_addr[3],tmp_data[1],tmp_data[0]);
    return NO_ERR;
}

int hx83102_determin_diag_rawdata(int diag_command)
{
    return diag_command%10;
}

int hx83102_determin_diag_storage(int diag_command)
{
    return diag_command/10;
}

void himax_reload_disable(struct i2c_client *client,int on)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];	

	I("%s:entering\n",__func__);

    if(on)/*reload disable*/
    {
		tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x7F; tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0xA5; tmp_data[0] = 0x5A;
	}
    else/*reload enable*/
    {
	    tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x7F; tmp_addr[0] = 0x00;
		tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
	}
	
	himax_flash_write_burst(client, tmp_addr, tmp_data);
		
	I("%s: setting OK!\n",__func__);	
}

int hx83102_switch_mode(struct i2c_client *client,int mode)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t mode_wirte_cmd;
    uint8_t mode_read_cmd;
    int result = -1;
    int retry = 200;

    I("%s: Entering\n",__func__);

    if(mode == 0) /*normal mode*/
    {
        mode_wirte_cmd = 0x00;
        mode_read_cmd = 0x99;
    }
    else		/*sorting mode*/
    {
        mode_wirte_cmd = 0xAA;
        mode_read_cmd = 0xCC;
    }

    hx83102_sense_off(client);

    //himax_interface_on(client);

    /*  */

    /* clean up FW status */
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x00;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;
    himax_flash_write_burst(client, tmp_addr, tmp_data);
    //msleep(30);

    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x07;
    tmp_addr[0] = 0xFC;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = mode_wirte_cmd;
    tmp_data[0] = mode_wirte_cmd;
    himax_flash_write_burst(client, tmp_addr, tmp_data);
    //msleep(30);

    hx83102_idle_mode(client,1);
	himax_reload_disable(client,1);

    // To stable the sorting //skip frames
    if(mode)
    {
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x70;
        tmp_addr[0] = 0xF4;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x08;
        himax_flash_write_burst(client, tmp_addr, tmp_data);
    }
    else
    {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x72;
		tmp_addr[0] = 0x94;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x0A;/*0x0A normal mode 10 frame*/
		/* N Frame Sorting*/
		himax_flash_write_burst(client, tmp_addr, tmp_data);
		
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x70;
        tmp_addr[0] = 0xF4;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x14;
        himax_flash_write_burst(client, tmp_addr, tmp_data);
    }

    hx83102_sense_on(client,0x01);
    I("mode_wirte_cmd(0)=0x%2.2X,mode_wirte_cmd(1)=0x%2.2X\n",tmp_data[0],tmp_data[1]);
    while(retry!=0)
    {
        I("[%d]Read 10007F04!\n",retry);
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x07;
        tmp_addr[0] = 0xFC;
        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
        msleep(100);
        I("mode_read_cmd(0)=0x%2.2X,mode_read_cmd(1)=0x%2.2X\n",tmp_data[0],tmp_data[1]);
        if(tmp_data[0] == mode_read_cmd && tmp_data[1] == mode_read_cmd)
        {
            I("Read OK!\n");
            result = 0;
            break;
        }
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xA8;
        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
        if(tmp_data[0] == 0x00 && tmp_data[1] == 0x00 && tmp_data[2] == 0x00 && tmp_data[3] == 0x00)
        {
            E("%s,: FW Stop!\n",__func__);
            break;
        }
        retry--;
    }

    if(result == 0)
    {
        if(mode == 0)
            return 1;  //normal mode
        else
            return 2;  //sorting mode
    }
    else
        return -1;    //change mode fail
}

void hx83102_return_event_stack(struct i2c_client *client)
{
    int retry = 20;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    I("%s:entering\n",__func__);
    do
    {

        I("%s,now %d times\n!",__func__,retry);
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x00;
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
        retry--;
        msleep(10);

    }
    while((tmp_data[1] != 0x00 && tmp_data[0] != 0x00) && retry > 0);

    I("%s: End of setting!\n",__func__);

}

void hx83102_diag_register_set(struct i2c_client *client, uint8_t diag_command)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    I("diag_command = %d\n", diag_command );

    himax_interface_on(client);

    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x02;
    tmp_addr[1] = 0x04;
    tmp_addr[0] = 0xB4;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = diag_command;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
    I("%s: tmp_data[3]=0x%02X,tmp_data[2]=0x%02X,tmp_data[1]=0x%02X,tmp_data[0]=0x%02X!\n",
      __func__,tmp_data[3],tmp_data[2],tmp_data[1],tmp_data[0]);

}

static void himax_init_psl(struct i2c_client *client) //power saving level
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    //==============================================================
    // SCU_Power_State_PW : 0x9000_00A0 ==> 0x0000_0000 (Reset PSL)
    //==============================================================
    tmp_addr[3] = 0x90;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0xA0;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;
    hx83102_register_write(client, tmp_addr, 4, tmp_data, false);

    I("%s: power saving level reset OK!\n",__func__);
}

void hx83102_flash_dump_func(struct i2c_client *client, uint8_t local_flash_command, int Flash_Size, uint8_t *flash_buffer)
{
    uint8_t tmp_addr[4];    
    uint8_t buffer[256];
    int page_prog_start = 0;

    hx83102_sense_off(client);
    hx83102_burst_enable(client, 1);

    for (page_prog_start = 0; page_prog_start < Flash_Size; page_prog_start = page_prog_start + 128)
    {
  
        tmp_addr[0] = page_prog_start % 0x100;
        tmp_addr[1] = (page_prog_start >> 8) % 0x100;
        tmp_addr[2] = (page_prog_start >> 16) % 0x100;
        tmp_addr[3] = page_prog_start / 0x1000000;
        hx83102_register_read(client, tmp_addr,128,buffer,false);
        memcpy(&flash_buffer[page_prog_start],buffer,128);
    }

    hx83102_burst_enable(client, 0);
    hx83102_sense_on(client, 0x01);

    return;

}

int hx83102_chip_self_test(struct i2c_client *client)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[128];
    uint8_t self_test_info[24];
    int pf_value=0x00;
    uint8_t test_result_id = 0;
    int i;

    memset(tmp_addr, 0x00, sizeof(tmp_addr));
    memset(tmp_data, 0x00, sizeof(tmp_data));

    himax_interface_on(client);
    hx83102_sense_off(client);


    hx83102_burst_enable(client, 1);

    // 0x100007f8 -> 0x00006AA6
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x07;
    tmp_addr[0] = 0xf8;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x6A;
    tmp_data[0] = 0xA6;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    //Set criteria 0x10007F1C [0,1]=aa/up,down=, [2-3]=key/up,down, [4]=iir_max_ulmt
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x7F;
    tmp_addr[0] = 0x1C;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0xFF;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0xFF;
    tmp_data[7] = 0x00;
    tmp_data[6] = 0x00;
    tmp_data[5] = 0x00;
    tmp_data[4] = 0xFF;
    himax_flash_write_burst_lenth(client, tmp_addr, tmp_data, 8);

    // 0x10007294 -> 0x0000190  //SET IIR_MAX FRAMES
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x72;
    tmp_addr[0] = 0x94;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x01;
    tmp_data[0] = 0x90;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    //Disable IDLE Mode
    hx83102_idle_mode(client,1);
	himax_reload_disable(client,1);

    //start selftest // leave safe mode
    hx83102_sense_on(client, 1);

    //Hand shaking -> 0x100007f8 waiting 0xA66A
    for(i = 0; i < 1000; i++)
    {
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x07;
        tmp_addr[0] = 0xf8;
        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
        I("%s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X,tmp_data[3] = 0x%02X, cnt=%d\n", __func__,tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3],i);
        msleep(10);
        if(tmp_data[1] == 0xA6 && tmp_data[0] == 0x6A )
        {
            I("%s Data ready goto moving data\n", __func__);
            break;
        }
    }

    hx83102_sense_off(client);
    msleep(20);

    //=====================================
    // Read test result ID : 0x10007f24 ==> bit[2][1][0] = [key][AA][avg] => 0xF = PASS
    //=====================================
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x7F;
    tmp_addr[0] = 0x24;
    hx83102_register_read(client, tmp_addr, 24, self_test_info, false);

    test_result_id = self_test_info[0];

    I("%s: check test result, test_result_id=%x, test_result=%x\n", __func__
      ,test_result_id,self_test_info[0]);

    I("raw top 1 = %d\n",self_test_info[3]*256+self_test_info[2]);
    I("raw top 2 = %d\n",self_test_info[5]*256+self_test_info[4]);
    I("raw top 3 = %d\n",self_test_info[7]*256+self_test_info[6]);

    I("raw last 1 = %d\n",self_test_info[9]*256+self_test_info[8]);
    I("raw last 2 = %d\n",self_test_info[11]*256+self_test_info[10]);
    I("raw last 3 = %d\n",self_test_info[13]*256+self_test_info[12]);

    I("raw key 1 = %d\n",self_test_info[15]*256+self_test_info[14]);
    I("raw key 2 = %d\n",self_test_info[17]*256+self_test_info[16]);
    I("raw key 3 = %d\n",self_test_info[19]*256+self_test_info[18]);

    if (test_result_id==0xAA)
    {
        I("[Himax]: self-test pass\n");
        pf_value = 0x1;
    }
    else
    {
        E("[Himax]: self-test fail\n");
        /* E("[Himax]: bank_avg = %d, bank_max = %d,%d,%d, bank_min = %d,%d,%d, key = %d,%d,%d\n",
        tmp_data[1],tmp_data[2],tmp_data[3],tmp_data[4],tmp_data[5],tmp_data[6],tmp_data[7],
        tmp_data[8],tmp_data[9],tmp_data[10]); */
        pf_value = 0x0;
    }

    //Enable IDLE Mode
    hx83102_idle_mode(client,0);
	himax_reload_disable(client,0);

    hx83102_sense_on(client, 0);
    msleep(120);

    return pf_value;
}

void hx83102_set_HSEN_enable(struct i2c_client *client, uint8_t HSEN_enable, bool suspended)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
	//Enable:0x10007F14 = 0xA55AA55A

	if(HSEN_enable)
	{
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x14;
		tmp_data[3] = 0xA5;
		tmp_data[2] = 0x5A;
		tmp_data[1] = 0xA5;
		tmp_data[0] = 0x5A;
		himax_flash_write_burst(client, tmp_addr, tmp_data);
	}
	else
	{
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0x14;
		tmp_data[3] = 0x00;
		tmp_data[2] = 0x00;
		tmp_data[1] = 0x00;
		tmp_data[0] = 0x00;
		himax_flash_write_burst(client, tmp_addr, tmp_data);
	}
}

int hx83102_palm_detect(uint8_t *buf)
{

    return GESTURE_DETECT_FAIL;

}

void hx83102_set_SMWP_enable(struct i2c_client *client, uint8_t SMWP_enable, bool suspended)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t back_data[4];
    uint8_t retry_cnt = 0;

	hx83102_sense_off(client);

    //Enable:0x10007F10 = 0xA55AA55A
    do
    {
        if(SMWP_enable)
        {
            tmp_addr[3] = 0x10;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x7F;
            tmp_addr[0] = 0x10;
            tmp_data[3] = 0xA5;
            tmp_data[2] = 0x5A;
            tmp_data[1] = 0xA5;
            tmp_data[0] = 0x5A;
            himax_flash_write_burst(client, tmp_addr, tmp_data);
            back_data[3] = 0XA5;
            back_data[2] = 0X5A;
            back_data[1] = 0XA5;
            back_data[0] = 0X5A;
        }
        else
        {
            tmp_addr[3] = 0x10;
            tmp_addr[2] = 0x00;
            tmp_addr[1] = 0x7F;
            tmp_addr[0] = 0x10;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(client, tmp_addr, tmp_data);
            back_data[3] = 0X00;
            back_data[2] = 0X00;
            back_data[1] = 0X00;
            back_data[0] = 0x00;
        }
        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
        //I("%s: tmp_data[0]=%d, SMWP_enable=%d, retry_cnt=%d \n", __func__, tmp_data[0],SMWP_enable,retry_cnt);
        retry_cnt++;
    }
    while((tmp_data[3] != back_data[3] || tmp_data[2] != back_data[2] || tmp_data[1] != back_data[1]  || tmp_data[0] != back_data[0] ) && retry_cnt < HIMAX_REG_RETRY_TIMES);

	hx83102_sense_on(client,0);

}

void hx83102_usb_detect_set(struct i2c_client *client,uint8_t *cable_config)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

	//Enable:0x10007F38 = 0xA55AA55A

    if(cable_config[1] == 0x01)
    {
       tmp_addr[3] = 0x10;
       tmp_addr[2] = 0x00;
       tmp_addr[1] = 0x7F;
       tmp_addr[0] = 0x38;
       tmp_data[3] = 0xA5;
       tmp_data[2] = 0x5A;
       tmp_data[1] = 0xA5;
       tmp_data[0] = 0x5A;
       himax_flash_write_burst(client, tmp_addr, tmp_data);
    }
    else
    {
       tmp_addr[3] = 0x10;
       tmp_addr[2] = 0x00;
       tmp_addr[1] = 0x7F;
       tmp_addr[0] = 0x38;
       tmp_data[3] = 0x77;
       tmp_data[2] = 0x88;
       tmp_data[1] = 0x77;
       tmp_data[0] = 0x88;
       himax_flash_write_burst(client, tmp_addr, tmp_data);
     }
}

void hx83102_burst_enable(struct i2c_client *client, uint8_t auto_add_4_byte)
{
    uint8_t tmp_data[4];

    tmp_data[0] = 0x31;
    if ( hx83102_i2c_himax_write(client, 0x13,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
    {
        E("%s: i2c access fail!\n", __func__);
        return;
    }

    tmp_data[0] = (0x10 | auto_add_4_byte);
    if ( hx83102_i2c_himax_write(client, 0x0D,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
    {
        E("%s: i2c access fail!\n", __func__);
        return;
    }

    //isBusrtOn = true;
}

int hx83102_register_read(struct i2c_client *client, uint8_t *read_addr, int read_length, uint8_t *read_data, bool hx83102_cfg_flag)
{
    uint8_t tmp_data[4];
    int i = 0;
    int address = 0;

    if(hx83102_cfg_flag == false)
    {
        if(read_length>256)
        {
            E("%s: read len over 256!\n", __func__);
            return LENGTH_FAIL;
        }
        if (read_length > 4)
            hx83102_burst_enable(client, 1);
        else
            hx83102_burst_enable(client, 0);

        address = (read_addr[3] << 24) + (read_addr[2] << 16) + (read_addr[1] << 8) + read_addr[0];
        i = address;
        tmp_data[0] = (uint8_t)i;
        tmp_data[1] = (uint8_t)(i >> 8);
        tmp_data[2] = (uint8_t)(i >> 16);
        tmp_data[3] = (uint8_t)(i >> 24);
        if ( hx83102_i2c_himax_write(client, 0x00,tmp_data, 4, DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return I2C_FAIL;
        }
        tmp_data[0] = 0x00;
        if ( hx83102_i2c_himax_write(client, 0x0C,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return I2C_FAIL;
        }

        if ( hx83102_i2c_himax_read(client, 0x08,read_data, read_length, DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return I2C_FAIL;
        }
        if (read_length > 4)
            hx83102_burst_enable(client, 0);
    }
    else
    {
        if(hx83102_i2c_himax_read(client, read_addr[0], read_data,read_length,DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return I2C_FAIL;
        }
    }
	return 0;
}

void hx83102_register_write(struct i2c_client *client, uint8_t *write_addr, int write_length, uint8_t *write_data, bool hx83102_cfg_flag)
{
    int i =0, address = 0;
    if(hx83102_cfg_flag == false)
    {
        address = (write_addr[3] << 24) + (write_addr[2] << 16) + (write_addr[1] << 8) + write_addr[0];

        for (i = address; i < address + write_length; i++)
        {
            if (write_length > 4)
            {
                hx83102_burst_enable(client, 1);
            }
            else
            {
                hx83102_burst_enable(client, 0);
            }
            himax_flash_write_burst_lenth(client, write_addr, write_data, write_length);
        }
    }
    else if(hx83102_cfg_flag == true)
    {
        if(hx83102_i2c_himax_write(client, write_addr[0], write_data,write_length,DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return;
        }
    }
    else
    {
        E("%s: hx83102_cfg_flag = %d, value is wrong!\n", __func__,hx83102_cfg_flag);
        return;
    }
}

bool hx83102_sense_off(struct i2c_client *client)
{
    uint8_t cnt = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
	
	//=====================================
	// FW ISR=0
	//=====================================
	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x5C;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0xA5;
	
	himax_flash_write_burst(client, tmp_addr, tmp_data);
	msleep(20);

    do
    {
        //===========================================
        //  0x31 ==> 0x27
        //===========================================
        tmp_data[0] = 0x27;
        if ( hx83102_i2c_himax_write(client, 0x31,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return false;
        }
        //===========================================
        //  0x32 ==> 0x95
        //===========================================
        tmp_data[0] = 0x95;
        if ( hx83102_i2c_himax_write(client, 0x32,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return false;
        }

        // ======================
        // Check enter_save_mode
        // ======================
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xA8;
        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);

        I("%s: Check enter_save_mode data[0]=%X \n", __func__,tmp_data[0]);

        if (tmp_data[0] == 0x0C)
        {
            //=====================================
            // Reset TCON
            //=====================================
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x02;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x20;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(client, tmp_addr, tmp_data);
            msleep(1);
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x01;
            himax_flash_write_burst(client, tmp_addr, tmp_data);

            //=====================================
            // Reset ADC
            //=====================================
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x02;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x94;
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x00;
            himax_flash_write_burst(client, tmp_addr, tmp_data);
            msleep(1);
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = 0x01;
            himax_flash_write_burst(client, tmp_addr, tmp_data);
            return true;
        }
        else
        {
            msleep(10);
#ifdef HX_RST_PIN_FUNC
			hx83102_ic_reset(false,false);
#endif
        }
    }
    while (cnt++ < 15);

    return false;
}

static bool wait_wip(struct i2c_client *client, int Timing)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t in_buffer[10];
    //uint8_t out_buffer[20];
    int retry_cnt = 0;

    //=====================================
    // SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
    //=====================================
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x10;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x02;
    tmp_data[1] = 0x07;
    tmp_data[0] = 0x80;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    in_buffer[0] = 0x01;

    do
    {
        //=====================================
        // SPI Transfer Control : 0x8000_0020 ==> 0x4200_0003
        //=====================================
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x20;
        tmp_data[3] = 0x42;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x03;
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        //=====================================
        // SPI Command : 0x8000_0024 ==> 0x0000_0005
        // read 0x8000_002C for 0x01, means wait success
        //=====================================
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x24;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x05;
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        in_buffer[0] = in_buffer[1] = in_buffer[2] = in_buffer[3] = 0xFF;
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x2C;
        hx83102_register_read(client, tmp_addr, 4, in_buffer, false);

        if ((in_buffer[0] & 0x01) == 0x00)
            return true;

        retry_cnt++;

        if (in_buffer[0] != 0x00 || in_buffer[1] != 0x00 || in_buffer[2] != 0x00 || in_buffer[3] != 0x00)
            I("%s:Wait wip retry_cnt:%d, buffer[0]=%d, buffer[1]=%d, buffer[2]=%d, buffer[3]=%d \n", __func__,
              retry_cnt,in_buffer[0],in_buffer[1],in_buffer[2],in_buffer[3]);

        if (retry_cnt > 100)
        {
            E("%s: Wait wip error!\n", __func__);
            return false;
        }
        msleep(Timing);
    }
    while ((in_buffer[0] & 0x01) == 0x01);
    return true;
}

void hx83102_sense_on(struct i2c_client *client, uint8_t FlashMode)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
	int retry = 0;

    I("Enter %s  \n", __func__);

    himax_interface_on(client);
	//=====================================
	// Clear FW ISR
	//=====================================
	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x5C;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x00;

	himax_flash_write_burst(client, tmp_addr, tmp_data);
	msleep(20);

	if(!FlashMode)
		{
#ifdef HX_RST_PIN_FUNC
			hx83102_ic_reset(false,false);
#else
			//===AHBI2C_SystemReset==========
			tmp_addr[3] = 0x90;
			tmp_addr[2] = 0x00;
			tmp_addr[1] = 0x00;
			tmp_addr[0] = 0x18;
			tmp_data[3] = 0x00;
			tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			tmp_data[0] = 0x55;
			hx83102_register_write(client, tmp_addr, 4, tmp_data, false);
#endif
		}
	else
		{
		    do
		    {
		        tmp_addr[3] = 0x90;
		        tmp_addr[2] = 0x00;
		        tmp_addr[1] = 0x00;
		        tmp_addr[0] = 0x98;
		        tmp_data[3] = 0x00;
		        tmp_data[2] = 0x00;
		        tmp_data[1] = 0x00;
		        tmp_data[0] = 0x53;
		        hx83102_register_write(client, tmp_addr, 4, tmp_data, false);

		        tmp_addr[0] = 0xE4;
		        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);

		        I("%s:Read status from IC = %X,%X\n", __func__, tmp_data[0],tmp_data[1]);

		    }
		    while((tmp_data[1] != 0x01 || tmp_data[0] != 0x00) && retry++ < 5);

		    if(retry >= 5)
		    {
		        E("%s: Fail:\n", __func__);
#ifdef HX_RST_PIN_FUNC
				hx83102_ic_reset(false,false);
#else
				//===AHBI2C_SystemReset==========
				tmp_addr[3] = 0x90;
				tmp_addr[2] = 0x00;
				tmp_addr[1] = 0x00;
				tmp_addr[0] = 0x18;
				tmp_data[3] = 0x00;
				tmp_data[2] = 0x00;
				tmp_data[1] = 0x00;
				tmp_data[0] = 0x55;
				hx83102_register_write(client, tmp_addr, 4, tmp_data, false);
#endif
		    }
		    else
		    {
		        I("%s:OK and Read status from IC = %X,%X\n", __func__, tmp_data[0],tmp_data[1]);

		        /* reset code*/
		        tmp_data[0] = 0x00;
		        if ( hx83102_i2c_himax_write(client, 0x31,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
		        {
		            E("%s: i2c access fail!\n", __func__);
		        }
		        if ( hx83102_i2c_himax_write(client, 0x32,tmp_data, 1, DEFAULT_RETRY_CNT) < 0)
		        {
		            E("%s: i2c access fail!\n", __func__);
		        }

		        tmp_addr[3] = 0x90;
		        tmp_addr[2] = 0x00;
		        tmp_addr[1] = 0x00;
		        tmp_addr[0] = 0x98;
		        tmp_data[3] = 0x00;
		        tmp_data[2] = 0x00;
		        tmp_data[1] = 0x00;
		        tmp_data[0] = 0x00;
		        hx83102_register_write(client, tmp_addr, 4, tmp_data, false);
		    }
		}
}
#if 0
static void himax_chip_erase(struct i2c_client *client)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    himax_interface_on(client);

    /* init psl */
    himax_init_psl(client);

    //=====================================
    // SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
    //=====================================
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x10;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x02;
    tmp_data[1] = 0x07;
    tmp_data[0] = 0x80;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    //=====================================
    // Chip Erase
    // Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
    //				  2. 0x8000_0024 ==> 0x0000_0006
    //=====================================
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x20;
    tmp_data[3] = 0x47;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x24;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x06;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    //=====================================
    // Chip Erase
    // Erase Command : 0x8000_0024 ==> 0x0000_00C7
    //=====================================
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x24;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0xC7;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    msleep(2000);

    if (!wait_wip(client, 100))
        E("%s:83102_Chip_Erase Fail\n", __func__);

}
#endif
static bool himax_block_erase(struct i2c_client *client, int start_addr, int length)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint32_t page_prog_start = 0;
	
	himax_interface_on(client);

	/* init psl */
	himax_init_psl(client);

	//=====================================
	// SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
	//=====================================
	tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x10;
	tmp_data[3] = 0x00; tmp_data[2] = 0x02; tmp_data[1] = 0x07; tmp_data[0] = 0x80;
	himax_flash_write_burst(client, tmp_addr, tmp_data);
	for (page_prog_start = start_addr; page_prog_start < start_addr + length; page_prog_start = page_prog_start + 0x10000)
		{
			//=====================================
			// Chip Erase
			// Write Enable :	1. 0x8000_0020 ==> 0x4700_0000//control
			//				2. 0x8000_0024 ==> 0x0000_0006//WREN
			//=====================================
			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
			tmp_data[3] = 0x47; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
			himax_flash_write_burst(client, tmp_addr, tmp_data);

			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
			tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x06;
			himax_flash_write_burst(client, tmp_addr, tmp_data);

			//=====================================
			// Block Erase
			// Erase Command : 0x8000_0028 ==> 0x0000_0000 //SPI addr
			//				   0x8000_0020 ==> 0x6700_0000 //control
			//				   0x8000_0024 ==> 0x0000_00D8 //BE
			//=====================================
			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x28;
			//tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;// Flash start address 1st : 0x0000_0000
			tmp_data[3] = (page_prog_start >> 24)&0xFF;
			tmp_data[2] = (page_prog_start >> 16)&0xFF;
			tmp_data[1] = (page_prog_start >> 8)&0xFF;
			tmp_data[0] = page_prog_start&0xFF;
			himax_flash_write_burst(client, tmp_addr, tmp_data);
			
			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
			tmp_data[3] = 0x67; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00;
			himax_flash_write_burst(client, tmp_addr, tmp_data);
			
			tmp_addr[3] = 0x80; tmp_addr[2] = 0x00; tmp_addr[1] = 0x00; tmp_addr[0] = 0x24;
			tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0xD8;
			himax_flash_write_burst(client, tmp_addr, tmp_data);
	
			msleep(1000);

			if (!wait_wip(client, 100))
			{
				E("%s:83102_Erase Fail\n", __func__);
				return false;
			}
		}
	return true;
}
#if 0
static bool himax_sector_erase(struct i2c_client *client, int start_addr)
{
	return true;
}

static void himax_sram_write(struct i2c_client *client, uint8_t *FW_content)
{

}

static bool himax_sram_verify(struct i2c_client *client, uint8_t *FW_File, int FW_Size)
{
	return true;
}
#endif
static void himax_flash_programming(struct i2c_client *client, uint8_t *FW_content, int FW_Size)
{
    int page_prog_start = 0;
    int program_length = 48;
    int i = 0, j = 0, k = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t buring_data[256];    // Read for flash data, 128K
    // 4 bytes for 0x80002C padding

    himax_interface_on(client);

    //=====================================
    // SPI Transfer Format : 0x8000_0010 ==> 0x0002_0780
    //=====================================
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x10;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x02;
    tmp_data[1] = 0x07;
    tmp_data[0] = 0x80;
    himax_flash_write_burst(client, tmp_addr, tmp_data);

    for (page_prog_start = 0; page_prog_start < FW_Size; page_prog_start = page_prog_start + 256)
    {
        //msleep(5);
        //=====================================
        // Write Enable : 1. 0x8000_0020 ==> 0x4700_0000
        //				  2. 0x8000_0024 ==> 0x0000_0006
        //=====================================
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x20;
        tmp_data[3] = 0x47;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x00;
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x24;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x06;
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        //=================================
        // SPI Transfer Control
        // Set 256 bytes page write : 0x8000_0020 ==> 0x610F_F000
        // Set read start address	: 0x8000_0028 ==> 0x0000_0000
        //=================================
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x20;
        tmp_data[3] = 0x61;
        tmp_data[2] = 0x0F;
        tmp_data[1] = 0xF0;
        tmp_data[0] = 0x00;
        // data bytes should be 0x6100_0000 + ((word_number)*4-1)*4096 = 0x6100_0000 + 0xFF000 = 0x610F_F000
        // Programmable size = 1 page = 256 bytes, word_number = 256 byte / 4 = 64
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x28;
        //tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0x00; tmp_data[0] = 0x00; // Flash start address 1st : 0x0000_0000

        if (page_prog_start < 0x100)
        {
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = 0x00;
            tmp_data[0] = (uint8_t)page_prog_start;
        }
        else if (page_prog_start >= 0x100 && page_prog_start < 0x10000)
        {
            tmp_data[3] = 0x00;
            tmp_data[2] = 0x00;
            tmp_data[1] = (uint8_t)(page_prog_start >> 8);
            tmp_data[0] = (uint8_t)page_prog_start;
        }
        else if (page_prog_start >= 0x10000 && page_prog_start < 0x1000000)
        {
            tmp_data[3] = 0x00;
            tmp_data[2] = (uint8_t)(page_prog_start >> 16);
            tmp_data[1] = (uint8_t)(page_prog_start >> 8);
            tmp_data[0] = (uint8_t)page_prog_start;
        }

        himax_flash_write_burst(client, tmp_addr, tmp_data);


        //=================================
        // Send 16 bytes data : 0x8000_002C ==> 16 bytes data
        //=================================
        buring_data[0] = 0x2C;
        buring_data[1] = 0x00;
        buring_data[2] = 0x00;
        buring_data[3] = 0x80;

        for (i = /*0*/page_prog_start, j = 0; i < 16 + page_prog_start/**/; i++, j++)	/// <------ bin file
        {
            buring_data[j + 4] = FW_content[i];
        }


        if ( hx83102_i2c_himax_write(client, 0x00,buring_data, 20, DEFAULT_RETRY_CNT) < 0)
        {
            E("%s: i2c access fail!\n", __func__);
            return;
        }
        //=================================
        // Write command : 0x8000_0024 ==> 0x0000_0002
        //=================================
        tmp_addr[3] = 0x80;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x24;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x02;
        himax_flash_write_burst(client, tmp_addr, tmp_data);

        //=================================
        // Send 240 bytes data : 0x8000_002C ==> 240 bytes data
        //=================================

        for (j = 0; j < 5; j++)
        {
            for (i = (page_prog_start + 16 + (j * 48)), k = 0; i < (page_prog_start + 16 + (j * 48)) + program_length; i++, k++)   /// <------ bin file
            {
                buring_data[k+4] = FW_content[i];//(byte)i;
            }

            if ( hx83102_i2c_himax_write(client, 0x00,buring_data, program_length+4, DEFAULT_RETRY_CNT) < 0)
            {
                E("%s: i2c access fail!\n", __func__);
                return;
            }

        }

        if (!wait_wip(client, 1))
            E("%s:83102_Flash_Programming Fail\n", __func__);
    }
}
#if 0
static bool Calculate_CRC_with_AP(unsigned char *FW_content, int CRC_from_FW, int mode)
{
	return true;
}
#endif
static uint32_t himax_hw_check_CRC(struct i2c_client *client, uint8_t *start_addr, int reload_length)
{
    uint32_t result = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    int cnt = 0, ret = 0;
    int length = reload_length / 4;

    //CRC4 // 0x8005_0020 <= from, 0x8005_0028 <= 0x0099_length
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x05;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x20;
    //tmp_data[3] = 0x00; tmp_data[2] = 0x00; tmp_data[1] = 0xFB; tmp_data[0] = 0x00;
    ret = himax_flash_write_burst(client, tmp_addr, start_addr);
	if ( ret < 0)
		{
			E("%s: i2c access fail!\n", __func__);
			return 1;
		}

    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x05;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x28;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x99;
    tmp_data[1] = (length >> 8);
    tmp_data[0] = length;
    ret = himax_flash_write_burst(client, tmp_addr, tmp_data);
	if ( ret < 0)
		{
			E("%s: i2c access fail!\n", __func__);
			return 1;
		}

    cnt = 0;
    tmp_addr[3] = 0x80;
    tmp_addr[2] = 0x05;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x00;
    do
    {
        ret = hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
		if ( ret < 0)
			{
				E("%s: i2c access fail!\n", __func__);
				return 1;
			}

        if ((tmp_data[0] & 0x01) != 0x01)
        {
            tmp_addr[3] = 0x80;
            tmp_addr[2] = 0x05;
            tmp_addr[1] = 0x00;
            tmp_addr[0] = 0x18;
            ret = hx83102_register_read(client, tmp_addr, 4, tmp_data, false);
			if ( ret < 0)
			{
				E("%s: i2c access fail!\n", __func__);
				return 1;
			}
            I("%s: tmp_data[3]=%X, tmp_data[2]=%X, tmp_data[1]=%X, tmp_data[0]=%X  \n", __func__, tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0]);
            result = ((tmp_data[3] << 24) + (tmp_data[2] << 16) + (tmp_data[1] << 8) + tmp_data[0]);
            break;
        }
    }
    while (cnt++ < 100);

    return result;
}
#if 0
static void himax_flash_page_write(struct i2c_client *client, uint8_t *write_addr, uint8_t *write_data)
{

}

static void himax_set_reload_cmd(uint8_t *write_data, int idx, uint32_t cmd_from, uint32_t cmd_to, uint32_t cmd_beat)
{
    int index = idx * 12;
    int i;
    for (i = 3; i >= 0; i--)
    {
        write_data[index + i] = (cmd_from >> (8 * i));
        write_data[index + 4 + i] = (cmd_to >> (8 * i));
        write_data[index + 8 + i] = (cmd_beat >> (8 * i));
    }
}

static bool himax_program_reload(struct i2c_client *client)
{
    return true;
}
#endif
int hx83102_fts_ctpm_fw_upgrade_with_sys_fs_32k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{
    /* Not use */
    return 0;
}

int hx83102_fts_ctpm_fw_upgrade_with_sys_fs_60k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{
    /* Not use */
    return 0;
}

int hx83102_fts_ctpm_fw_upgrade_with_sys_fs_64k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{

    //int CRC_from_FW = 0;
    int burnFW_success = 0;
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    if (len != FW_SIZE_64k)   //64k
    {
        E("%s: The file size is not 64K bytes\n", __func__);
        return false;
    }

#ifdef HX_RST_PIN_FUNC
	hx83102_ic_reset(false,false);
#else
	//===AHBI2C_SystemReset==========
	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x18;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x55;
	hx83102_register_write(client, tmp_addr, 4, tmp_data, false);
#endif

    hx83102_sense_off(client);
    //himax_chip_erase(client);
	himax_block_erase(client,0x00,FW_SIZE_64k);
    himax_flash_programming(client, fw, FW_SIZE_64k);

    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;

    if(himax_hw_check_CRC(client,tmp_data, FW_SIZE_64k) == 0)
    {
        burnFW_success = 1;
    }
    else
    {
        burnFW_success = 0;
    }
	/*RawOut select initial*/
	tmp_addr[3] = 0x80;tmp_addr[2] = 0x02;tmp_addr[1] = 0x04;tmp_addr[0] = 0xB4;
    tmp_data[3] = 0x00;tmp_data[2] = 0x00;tmp_data[1] = 0x00;tmp_data[0] = 0x00;
	hx83102_register_write(client,tmp_addr, 4, tmp_data, false);

	/*DSRAM func initial*/
    tmp_addr[3] = 0x10;tmp_addr[2] = 0x00;tmp_addr[1] = 0x07;tmp_addr[0] = 0xFC;
    tmp_data[3] = 0x00;tmp_data[2] = 0x00;tmp_data[1] = 0x00;tmp_data[0] = 0x00;
	hx83102_register_write(client,tmp_addr, 4, tmp_data, false);
	
#ifdef HX_RST_PIN_FUNC
	hx83102_ic_reset(false,false);
#else
	//===AHBI2C_SystemReset==========
	tmp_addr[3] = 0x90;
	tmp_addr[2] = 0x00;
	tmp_addr[1] = 0x00;
	tmp_addr[0] = 0x18;
	tmp_data[3] = 0x00;
	tmp_data[2] = 0x00;
	tmp_data[1] = 0x00;
	tmp_data[0] = 0x55;
	hx83102_register_write(client, tmp_addr, 4, tmp_data, false);
#endif
    return burnFW_success;

}

int hx83102_fts_ctpm_fw_upgrade_with_sys_fs_124k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{
    /* Not use */
    return 0;
}

int hx83102_fts_ctpm_fw_upgrade_with_sys_fs_128k(struct i2c_client *client, unsigned char *fw, int len, bool change_iref)
{
    /* Not use */
    return 0;
}

void hx83102_touch_information(struct i2c_client *client)
{
#ifndef HX_FIX_TOUCH_INFO
    uint8_t cmd[4];
    char data[12] = {0};

    I("%s:hx83102_IC_TYPE =%d\n", __func__,hx83102_IC_TYPE);

    if(hx83102_IC_TYPE == HX_83102A_SERIES_PWON)
    {
        cmd[3] = 0x10;
        cmd[2] = 0x00;
        cmd[1] = 0x70;
        cmd[0] = 0xF4;
        hx83102_register_read(client, cmd, 8, data, false);
        hx83102_ic_data->HX_RX_NUM				= data[2];
        hx83102_ic_data->HX_TX_NUM				= data[3];
        hx83102_ic_data->HX_MAX_PT				= data[4];
        //I("%s : HX_RX_NUM=%d,hx83102_ic_data->HX_TX_NUM=%d,hx83102_ic_data->HX_MAX_PT=%d\n",__func__,hx83102_ic_data->HX_RX_NUM,hx83102_ic_data->HX_TX_NUM,hx83102_ic_data->HX_MAX_PT);

        cmd[3] = 0x10;
        cmd[2] = 0x00;
        cmd[1] = 0x70;
        cmd[0] = 0xFA;
        hx83102_register_read(client, cmd, 4, data, false);
        //I("%s : c_data->HX_XY_REVERSE=0x%2.2X\n",__func__,data[1]);
        if((data[1] & 0x04) == 0x04)
        {
            hx83102_ic_data->HX_XY_REVERSE = true;
        }
        else
        {
            hx83102_ic_data->HX_XY_REVERSE = false;
        }

        cmd[3] = 0x10;
        cmd[2] = 0x00;
        cmd[1] = 0x70;
        cmd[0] = 0xFC;
        hx83102_register_read(client, cmd, 4, data, false);
        hx83102_ic_data->HX_Y_RES = data[0]*256;
        hx83102_ic_data->HX_Y_RES = hx83102_ic_data->HX_Y_RES + data[1];
        hx83102_ic_data->HX_X_RES = data[2]*256 + data[3];
        //I("%s : hx83102_ic_data->HX_Y_RES=%d,hx83102_ic_data->HX_X_RES=%d \n",__func__,hx83102_ic_data->HX_Y_RES,hx83102_ic_data->HX_X_RES);

        cmd[3] = 0x10;
        cmd[2] = 0x00;
        cmd[1] = 0x70;
        cmd[0] = 0x89;
        hx83102_register_read(client, cmd, 4, data, false);
        //I("%s : data[0]=0x%2.2X,data[1]=0x%2.2X,data[2]=0x%2.2X,data[3]=0x%2.2X\n",__func__,data[0],data[1],data[2],data[3]);
        //I("data[0] & 0x01 = %d\n",(data[0] & 0x01));
        if((data[0] & 0x01) == 1)
        {
            hx83102_ic_data->HX_INT_IS_EDGE = true;
        }
        else
        {
            hx83102_ic_data->HX_INT_IS_EDGE = false;
        }

        if (hx83102_ic_data->HX_RX_NUM > 40)
            hx83102_ic_data->HX_RX_NUM = 32;
        if (hx83102_ic_data->HX_TX_NUM > 20)
            hx83102_ic_data->HX_TX_NUM = 18;
        if (hx83102_ic_data->HX_MAX_PT > 10)
            hx83102_ic_data->HX_MAX_PT = 10;
        if (hx83102_ic_data->HX_Y_RES > 2000)
            hx83102_ic_data->HX_Y_RES = 1280;
        if (hx83102_ic_data->HX_X_RES > 2000)
            hx83102_ic_data->HX_X_RES = 720;

		/*Read number of MKey R100070E8H*/
		cmd[3] = 0x10;
		cmd[2] = 0x00;
		cmd[1] = 0x70;
		cmd[0] = 0xE8;
		hx83102_register_read(client, cmd, 4,data, false);
		//I("%s: data[0] = 0x%02X,data[1] = 0x%02X,data[2] = 0x%02X,data[3] = 0x%02X\n", __func__,data[0],data[1],data[2],data[3]);
		hx83102_ic_data->HX_BT_NUM = data[0] & 0x03;
    }
    else
    {
        hx83102_ic_data->HX_RX_NUM				= 0;
        hx83102_ic_data->HX_TX_NUM				= 0;
        hx83102_ic_data->HX_BT_NUM				= 0;
        hx83102_ic_data->HX_X_RES				= 0;
        hx83102_ic_data->HX_Y_RES				= 0;
        hx83102_ic_data->HX_MAX_PT				= 0;
        hx83102_ic_data->HX_XY_REVERSE		= false;
        hx83102_ic_data->HX_INT_IS_EDGE		= false;
    }
#else
    hx83102_ic_data->HX_RX_NUM				= FIX_HX_RX_NUM;
    hx83102_ic_data->HX_TX_NUM				= FIX_HX_TX_NUM;
    hx83102_ic_data->HX_BT_NUM				= FIX_HX_BT_NUM;
    hx83102_ic_data->HX_X_RES				= FIX_HX_X_RES;
    hx83102_ic_data->HX_Y_RES				= FIX_HX_Y_RES;
    hx83102_ic_data->HX_MAX_PT				= FIX_HX_MAX_PT;
    hx83102_ic_data->HX_XY_REVERSE			= FIX_HX_XY_REVERSE;
    hx83102_ic_data->HX_INT_IS_EDGE			= FIX_HX_INT_IS_EDGE;
#endif
    I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d \n", __func__,hx83102_ic_data->HX_RX_NUM,hx83102_ic_data->HX_TX_NUM,hx83102_ic_data->HX_MAX_PT);
    I("%s:HX_XY_REVERSE =%d,HX_Y_RES =%d,HX_X_RES=%d \n", __func__,hx83102_ic_data->HX_XY_REVERSE,hx83102_ic_data->HX_Y_RES,hx83102_ic_data->HX_X_RES);
    I("%s:HX_INT_IS_EDGE =%d \n", __func__,hx83102_ic_data->HX_INT_IS_EDGE);
}

int hx83102_read_i2c_status(struct i2c_client *client)
{
    return hx83102_i2c_error_count; //
}

int hx83102_read_ic_trigger_type(struct i2c_client *client)
{
    uint8_t cmd[4];
    char data[12] = {0};
    int trigger_type = false;
	
	hx83102_sense_off(client);

    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x89;
    hx83102_register_read(client, cmd, 4, data, false);
    if((data[0] & 0x01) == 1)
    {
        trigger_type = true;
    }
    else
    {
        trigger_type = false;
    }
	hx83102_sense_on(client,1);

    return trigger_type;
}

void hx83102_read_FW_ver(struct i2c_client *client)
{
    uint8_t cmd[4],cmd_2[4];
    uint8_t data[64],data_2[64];
	int retry = 200;
    int reload_status = 0;

	hx83102_sense_on(client,0);

	while(reload_status == 0)
	{
		cmd[3] = 0x10;
		cmd[2] = 0x00;
		cmd[1] = 0x7f;
		cmd[0] = 0x00;
		hx83102_register_read(client, cmd, 4, data, false);

		cmd_2[3] = 0x10;
		cmd_2[2] = 0x00;
		cmd_2[1] = 0x72;
		cmd_2[0] = 0xc0;
		hx83102_register_read(client, cmd_2, 4, data_2, false);

		if((data[3]==0x00 && data[2]==0x00 && data[1]==0x3A && data[0]==0xA3 )
			||(data_2[3]==0x00 && data_2[2]==0x00 && data_2[1]==0x72 && data_2[0]==0xC0 ))
		{
			I("reload OK! \n");
			reload_status = 1;
			break;
		}
		else if(retry == 0)
		{
			E("reload 20 times! fail \n");
			hx83102_ic_data->vendor_panel_ver = 0;
			hx83102_ic_data->vendor_fw_ver = 0;
			hx83102_ic_data->vendor_config_ver = 0;
			hx83102_ic_data->vendor_touch_cfg_ver = 0;
			hx83102_ic_data->vendor_display_cfg_ver = 0;
			hx83102_ic_data->vendor_cid_maj_ver = 0;
    		hx83102_ic_data->vendor_cid_min_ver = 0;
			return;
		}
		else
		{
			retry --;
			msleep(10);
			I("reload fail ,delay 10ms retry=%d\n",retry);
		}
	}
	I("%s : data[0]=0x%2.2X,data[1]=0x%2.2X,data[2]=0x%2.2X,data[3]=0x%2.2X\n",__func__,data[0],data[1],data[2],data[3]);
	I("reload_status=%d\n",reload_status);

	hx83102_sense_off(client);

    //=====================================
    // Read FW version : 0x1000_7004  but 05,06 are the real addr for FW Version
    //=====================================

    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x04;
    hx83102_register_read(client, cmd, 4, data, false);

    hx83102_ic_data->vendor_panel_ver =  data[0];
    hx83102_ic_data->vendor_fw_ver = data[1] << 8 | data[2];

    I("PANEL_VER : %X \n",hx83102_ic_data->vendor_panel_ver);
    I("FW_VER : %X \n",hx83102_ic_data->vendor_fw_ver);


    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x84;
    hx83102_register_read(client, cmd, 4, data, false);

    hx83102_ic_data->vendor_config_ver = data[2] << 8 | data[3];
    //I("CFG_VER : %X \n",hx83102_ic_data->vendor_config_ver);
    hx83102_ic_data->vendor_touch_cfg_ver = data[2];
    I("TOUCH_VER : %X \n",hx83102_ic_data->vendor_touch_cfg_ver);
    hx83102_ic_data->vendor_display_cfg_ver = data[3];
    I("DISPLAY_VER : %X \n",hx83102_ic_data->vendor_display_cfg_ver);


    cmd[3] = 0x10;
    cmd[2] = 0x00;
    cmd[1] = 0x70;
    cmd[0] = 0x00;
    hx83102_register_read(client, cmd, 4, data, false);

    hx83102_ic_data->vendor_cid_maj_ver = data[2] ;
    hx83102_ic_data->vendor_cid_min_ver = data[3];

    I("CID_VER : %X \n",(hx83102_ic_data->vendor_cid_maj_ver << 8 | hx83102_ic_data->vendor_cid_min_ver));
    return;
}

bool hx83102_ic_package_check(struct i2c_client *client)
{
    uint8_t tmp_addr[4];
    uint8_t tmp_data[4];
    uint8_t ret_data = 0x00;
    int i = 0;

	hx83102_sense_off(client);

    for (i = 0; i < 5; i++)
    {
// Product ID
        // Touch
        tmp_addr[3] = 0x90;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0xD0;
        hx83102_register_read(client, tmp_addr, 4, tmp_data, false);

        I("%s:Read driver IC ID = %X,%X,%X\n", __func__, tmp_data[3],tmp_data[2],tmp_data[1]);
        if ((tmp_data[3] == 0x83) && (tmp_data[2] == 0x10) && ((tmp_data[1] == 0x2a)||(tmp_data[1] == 0x2b)))
        {
            hx83102_IC_TYPE         		= HX_83102A_SERIES_PWON;
            hx83102_IC_CHECKSUM 			= HX_TP_BIN_CHECKSUM_CRC;
            //Himax: Set FW and CFG Flash Address
            HX83102_FW_VER_MAJ_FLASH_ADDR   = 49157;  //0x00C005
            HX83102_FW_VER_MAJ_FLASH_LENG   = 1;
            HX83102_FW_VER_MIN_FLASH_ADDR   = 49158;  //0x00C006
            HX83102_FW_VER_MIN_FLASH_LENG   = 1;
            HX83102_CFG_VER_MAJ_FLASH_ADDR 	= 49408;  //0x00C100
            HX83102_CFG_VER_MAJ_FLASH_LENG 	= 1;
            HX83102_CFG_VER_MIN_FLASH_ADDR 	= 49409;  //0x00C101
            HX83102_CFG_VER_MIN_FLASH_LENG 	= 1;
            HX83102_CID_VER_MAJ_FLASH_ADDR	= 49154;  //0x00C002
            HX83102_CID_VER_MAJ_FLASH_LENG	= 1;
            HX83102_CID_VER_MIN_FLASH_ADDR	= 49155;  //0x00C003
            HX83102_CID_VER_MIN_FLASH_LENG	= 1;
            //PANEL_VERSION_ADDR		= 49156;  //0x00C004
            //PANEL_VERSION_LENG		= 1;
#ifdef HX_AUTO_UPDATE_FW
            g_hx83102_i_FW_VER = hx83102_i_CTPM_FW[HX83102_FW_VER_MAJ_FLASH_ADDR]<<8 |hx83102_i_CTPM_FW[HX83102_FW_VER_MIN_FLASH_ADDR];
            g_hx83102_i_CFG_VER = hx83102_i_CTPM_FW[HX83102_CFG_VER_MAJ_FLASH_ADDR]<<8 |hx83102_i_CTPM_FW[HX83102_CFG_VER_MIN_FLASH_ADDR];
            g_hx83102_i_CID_MAJ = hx83102_i_CTPM_FW[HX83102_CID_VER_MAJ_FLASH_ADDR];
            g_hx83102_i_CID_MIN = hx83102_i_CTPM_FW[HX83102_CID_VER_MIN_FLASH_ADDR];
#endif
            I("Himax IC package 83102_in\n");
            ret_data = true;
            break;
        }
        else
        {
            ret_data = false;
            E("%s:Read driver ID register Fail:\n", __func__);
        }
    }

    return ret_data;
}

void hx83102_power_on_init(struct i2c_client *client)
{
	uint8_t tmp_addr[4];
    uint8_t tmp_data[4];

    I("%s:\n", __func__);
    hx83102_touch_information(client);

	/*RawOut select initial*/
	tmp_addr[3] = 0x80;tmp_addr[2] = 0x02;tmp_addr[1] = 0x04;tmp_addr[0] = 0xB4;
    tmp_data[3] = 0x00;tmp_data[2] = 0x00;tmp_data[1] = 0x00;tmp_data[0] = 0x00;
	hx83102_register_write(client,tmp_addr, 4, tmp_data, false);

	/*DSRAM func initial*/
    tmp_addr[3] = 0x10;tmp_addr[2] = 0x00;tmp_addr[1] = 0x07;tmp_addr[0] = 0xFC;
    tmp_data[3] = 0x00;tmp_data[2] = 0x00;tmp_data[1] = 0x00;tmp_data[0] = 0x00;
	hx83102_register_write(client,tmp_addr, 4, tmp_data, false);

	hx83102_sense_on(client, 0x00);
}

bool hx83102_read_event_stack(struct i2c_client *client, 	uint8_t *buf, uint8_t length)
{
    uint8_t cmd[4];

    //  AHB_I2C Burst Read Off
    cmd[0] = 0x00;
    if ( hx83102_i2c_himax_write(client, 0x11,cmd, 1, DEFAULT_RETRY_CNT) < 0)
    {
        E("%s: i2c access fail!\n", __func__);
        return 0;
    }

    hx83102_i2c_himax_read(client, 0x30, buf, length,DEFAULT_RETRY_CNT);

    //  AHB_I2C Burst Read On
    cmd[0] = 0x01;
    if ( hx83102_i2c_himax_write(client, 0x11,cmd, 1, 3) < 0)
    {
        E("%s: i2c access fail!\n", __func__);
        return 0;
    }
    return 1;
}

void hx83102_get_DSRAM_data(struct i2c_client *client, uint8_t *info_data)
{
    int i = 0;
    //int cnt = 0;
    unsigned char tmp_addr[4];
    unsigned char tmp_data[4];
    uint8_t max_i2c_size = 128;
    uint8_t x_num = hx83102_ic_data->HX_RX_NUM;
    uint8_t y_num = hx83102_ic_data->HX_TX_NUM;
    int m_key_num = 0;
    int total_size = (x_num * y_num + x_num + y_num) * 2 + 4;
    int total_size_temp;
    int mutual_data_size = x_num * y_num * 2;
    int total_read_times = 0;
    int address = 0;
    uint8_t  *temp_info_data; //max mkey size = 8
    uint16_t check_sum_cal = 0;
    int fw_run_flag = -1;
    //uint16_t temp_check_sum_cal = 0;

	temp_info_data = kzalloc(sizeof(uint8_t)*(total_size + 8),GFP_KERNEL);

    /*1. Read number of MKey R100070E8H to determin data size*/
    m_key_num = hx83102_ic_data->HX_BT_NUM;
    //I("%s,m_key_num=%d\n",__func__,m_key_num);
    total_size += m_key_num*2;

    /* 2. Start DSRAM Rawdata and Wait Data Ready */
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x00;
    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x5A;
    tmp_data[0] = 0xA5;
    fw_run_flag = himax_write_read_reg(client,tmp_addr,tmp_data,0xA5,0x5A);

    if(fw_run_flag < 0)
    {
        I("%s Data NOT ready => bypass \n", __func__);
		kfree(temp_info_data);
        return;
    }

    /* 3. Read RawData */
    total_size_temp = total_size;
    tmp_addr[3] = 0x10;
    tmp_addr[2] = 0x00;
    tmp_addr[1] = 0x00;
    tmp_addr[0] = 0x00;

    if (total_size % max_i2c_size == 0)
    {
        total_read_times = total_size / max_i2c_size;
    }
    else
    {
        total_read_times = total_size / max_i2c_size + 1;
    }

    for (i = 0; i < (total_read_times); i++)
    {
        if ( total_size_temp >= max_i2c_size)
        {
            hx83102_register_read(client, tmp_addr, max_i2c_size, &temp_info_data[i*max_i2c_size], false);
            total_size_temp = total_size_temp - max_i2c_size;
        }
        else
        {
            //I("last total_size_temp=%d\n",total_size_temp);
            hx83102_register_read(client, tmp_addr, total_size_temp % max_i2c_size, &temp_info_data[i*max_i2c_size], false);
        }

        address = ((i+1)*max_i2c_size);
        tmp_addr[1] = (uint8_t)((address>>8)&0x00FF);
        tmp_addr[0] = (uint8_t)((address)&0x00FF);
    }

    /* 4. FW stop outputing */
    //I("hx83102_DSRAM_Flag=%d\n",hx83102_DSRAM_Flag);
    if(hx83102_DSRAM_Flag == false)
    {
        //I("Return to Event Stack!\n");
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;
        tmp_data[3] = 0x00;
        tmp_data[2] = 0x00;
        tmp_data[1] = 0x00;
        tmp_data[0] = 0x00;
        himax_flash_write_burst(client, tmp_addr, tmp_data);
    }
    else
    {
        //I("Continue to SRAM!\n");
        tmp_addr[3] = 0x10;
        tmp_addr[2] = 0x00;
        tmp_addr[1] = 0x00;
        tmp_addr[0] = 0x00;
        tmp_data[3] = 0x11;
        tmp_data[2] = 0x22;
        tmp_data[1] = 0x33;
        tmp_data[0] = 0x44;
        himax_flash_write_burst(client, tmp_addr, tmp_data);
    }

    /* 5. Data Checksum Check */
    for (i = 2; i < total_size; i=i+2)/* 2:PASSWORD NOT included */
    {
        check_sum_cal += (temp_info_data[i+1]*256 + temp_info_data[i]);
    }

    if (check_sum_cal % 0x10000 != 0)
    {
        I("%s check_sum_cal fail=%2X \n", __func__, check_sum_cal);
		kfree(temp_info_data);
        return;
    }
    else
    {
        memcpy(info_data, &temp_info_data[4], mutual_data_size * sizeof(uint8_t));
        //I("%s checksum PASS \n", __func__);
    }
	kfree(temp_info_data);
}

bool hx83102_calculateChecksum(struct i2c_client *client, bool change_iref)
{
    uint8_t CRC_result = 0;
    uint8_t tmp_data[4];

    tmp_data[3] = 0x00;
    tmp_data[2] = 0x00;
    tmp_data[1] = 0x00;
    tmp_data[0] = 0x00;

    CRC_result = himax_hw_check_CRC(client,tmp_data, FW_SIZE_64k);

	msleep(50);

    return CRC_result;
}

bool hx83102_flash_lastdata_check(struct i2c_client *client)
{
	uint8_t tmp_addr[4];
	uint32_t start_addr = 0xFF80;
	uint32_t temp_addr = 0;
	uint32_t flash_page_len = 0x80;
	uint8_t flash_tmp_buffer[128];

	for (temp_addr = start_addr; temp_addr < (start_addr + flash_page_len); temp_addr = temp_addr + flash_page_len)
	{
		//I("temp_addr=%d,tmp_addr[0]=0x%2X,tmp_addr[1]=0x%2X,tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n",temp_addr,tmp_addr[0],tmp_addr[1],tmp_addr[2],tmp_addr[3]);
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		hx83102_register_read(client, tmp_addr ,flash_page_len, &flash_tmp_buffer[0], false);
	}

	if((!flash_tmp_buffer[flash_page_len-4]) && (!flash_tmp_buffer[flash_page_len-3]) && (!flash_tmp_buffer[flash_page_len-2]) && (!flash_tmp_buffer[flash_page_len-1]))
		{
			return 1;//FAIL
		}
	else
		{
			I("flash_buffer[FFFC]=0x%2X,flash_buffer[FFFD]=0x%2X,flash_buffer[FFFE]=0x%2X,flash_buffer[FFFF]=0x%2X\n",
				flash_tmp_buffer[flash_page_len-4],flash_tmp_buffer[flash_page_len-3],flash_tmp_buffer[flash_page_len-2],flash_tmp_buffer[flash_page_len-1]);
			return 0;//PASS
		}
}

//ts_work
int hx83102_cal_data_len(int raw_cnt_rmd, int HX_MAX_PT, int raw_cnt_max)
{
    int RawDataLen;
    if (raw_cnt_rmd != 0x00)
    {
        RawDataLen = 128 - ((HX_MAX_PT+raw_cnt_max+3)*4) - 1;
    }
    else
    {
        RawDataLen = 128 - ((HX_MAX_PT+raw_cnt_max+2)*4) - 1;
    }
    return RawDataLen;
}

bool hx83102_diag_check_sum( struct hx_83102_report_data *hx83102_touch_data ) //return checksum value
{
    uint16_t check_sum_cal = 0;
    int i;

    //Check 128th byte CRC
    for (i = 0, check_sum_cal = 0; i < (hx83102_touch_data->touch_all_size - hx83102_touch_data->touch_info_size); i=i+2)
    {
        check_sum_cal += (hx83102_touch_data->hx_rawdata_buf[i+1]*256 + hx83102_touch_data->hx_rawdata_buf[i]);
    }
    if (check_sum_cal % 0x10000 != 0)
    {
        I("%s fail=%2X \n", __func__, check_sum_cal);
        return 0;
        //goto bypass_checksum_failed_packet;
    }

    return 1;
}


void hx83102_diag_parse_raw_data(struct hx_83102_report_data *hx83102_touch_data,int mul_num, int self_num,uint8_t diag_cmd, int32_t *mutual_data, int32_t *self_data)
{
    int RawDataLen_word;
    int index = 0;
    int temp1, temp2,i;

    if (hx83102_touch_data->hx_rawdata_buf[0] == 0x3A
            && hx83102_touch_data->hx_rawdata_buf[1] == 0xA3
            && hx83102_touch_data->hx_rawdata_buf[2] > 0
            && hx83102_touch_data->hx_rawdata_buf[3] == diag_cmd )
    {
        RawDataLen_word = hx83102_touch_data->rawdata_size/2;
        index = (hx83102_touch_data->hx_rawdata_buf[2] - 1) * RawDataLen_word;
        //I("Header[%d]: %x, %x, %x, %x, mutual: %d, self: %d\n", index, buf[56], buf[57], buf[58], buf[59], mul_num, self_num);
        //I("RawDataLen=%d , RawDataLen_word=%d , hx_touch_info_size=%d\n", RawDataLen, RawDataLen_word, hx_touch_info_size);
        for (i = 0; i < RawDataLen_word; i++)
        {
            temp1 = index + i;

            if (temp1 < mul_num)
            {
                //mutual
                mutual_data[index + i] = ((int8_t)hx83102_touch_data->hx_rawdata_buf[i*2 + 4 + 1])*256 + hx83102_touch_data->hx_rawdata_buf[i*2 + 4];	//4: RawData Header, 1:HSB
            }
            else
            {
                //self
                temp1 = i + index;
                temp2 = self_num + mul_num;

                if (temp1 >= temp2)
                {
                    break;
                }
                self_data[i + index - mul_num] = (hx83102_touch_data->hx_rawdata_buf[i * 2 + 4 + 1] << 8) +
						hx83102_touch_data->hx_rawdata_buf[i * 2 + 4]; /* 4: RawData Header */
            }
        }
    }

}
uint8_t hx83102_read_DD_status(uint8_t *hx83102_cmd_set, uint8_t *tmp_data)
{
    int cnt = 0;
    uint8_t req_size = hx83102_cmd_set[0];
    uint8_t cmd_addr[4] = {0xFC, 0x00, 0x00, 0x90}; //0x900000FC -> cmd and hand shaking
    uint8_t tmp_addr[4] = {0x80, 0x7F, 0x00, 0x10}; //0x10007F80 -> data space

    hx83102_cmd_set[3] = 0xAA;
    hx83102_register_write(hx83102_ts->client, cmd_addr, 4, hx83102_cmd_set, 0);

    I("%s: hx83102_cmd_set[0] = 0x%02X,hx83102_cmd_set[1] = 0x%02X,hx83102_cmd_set[2] = 0x%02X,hx83102_cmd_set[3] = 0x%02X\n", __func__,hx83102_cmd_set[0],hx83102_cmd_set[1],hx83102_cmd_set[2],hx83102_cmd_set[3]);

    for (cnt = 0; cnt < 100; cnt++) //doing hand shaking 0xAA -> 0xBB
    {
        hx83102_register_read(hx83102_ts->client, cmd_addr, 4, tmp_data, false);
        //I("%s: tmp_data[0] = 0x%02X,tmp_data[1] = 0x%02X,tmp_data[2] = 0x%02X,tmp_data[3] = 0x%02X, cnt=%d\n", __func__,tmp_data[0],tmp_data[1],tmp_data[2],tmp_data[3],cnt);
        msleep(10);
        if(tmp_data[3] == 0xBB)
        {
            I("%s Data ready goto moving data\n", __func__);
            break;
        }
        else if(cnt >= 99)
        {
            I("%s Data not ready in FW \n", __func__);
            return FW_NOT_READY;
        }
    }
	hx83102_sense_off(hx83102_ts->client);
    hx83102_register_read(hx83102_ts->client, tmp_addr, req_size, tmp_data, false);
	hx83102_sense_on(hx83102_ts->client,1);
    return NO_ERR;
}

int hx83102_read_FW_status(uint8_t *state_addr, uint8_t *tmp_addr)
{
	uint8_t req_size = 0;
	uint8_t status_addr[4] = {0x44, 0x7F, 0x00, 0x10}; //0x10007F44
	uint8_t cmd_addr[4] = {0xF8, 0x00, 0x00, 0x90}; //0x900000F8	

	if(state_addr[0]==0x01)
		{
			state_addr[1]= 0x04;
			state_addr[2]= status_addr[0];state_addr[3]= status_addr[1];state_addr[4]= status_addr[2];state_addr[5]= status_addr[3];
			req_size = 0x04;
			hx83102_sense_off(hx83102_ts->client);
			hx83102_register_read(hx83102_ts->client, status_addr, req_size, tmp_addr, false);
			hx83102_sense_on(hx83102_ts->client,1);
		}
	else if(state_addr[0]==0x02)
		{
			state_addr[1]= 0x30;
			state_addr[2]= cmd_addr[0];state_addr[3]= cmd_addr[1];state_addr[4]= cmd_addr[2];state_addr[5]= cmd_addr[3];
			req_size = 0x30;
			hx83102_register_read(hx83102_ts->client, cmd_addr, req_size, tmp_addr, false);
		}

	return NO_ERR;
}

#ifdef HX_TP_PROC_GUEST_INFO
#define HX_GUEST_INFO_SIZE	10
#define HX_GUEST_INFO_LEN_SIZE	4
int g_guest_info_ongoing 			= 0; //0 stop //1 ongoing
char g_guest_str[10][128];



int hx83102_guest_info_get_status(void)
{
	return g_guest_info_ongoing;
}
void hx83102_guest_info_set_status(int setting)
{
	g_guest_info_ongoing = setting;
}

void himax_guest_info_read(uint32_t start_addr,uint8_t *flash_tmp_buffer)
{
	uint32_t temp_addr = 0;
	uint8_t tmp_addr[4];
	uint32_t flash_page_len = 0x1000;
	
	
	I("Reading guest info in start_addr = %d !\n",start_addr);
	for (temp_addr = start_addr; temp_addr < (start_addr + flash_page_len); temp_addr = temp_addr + 128)
	{
		
		//I("temp_addr=%d,tmp_addr[0]=0x%2X,tmp_addr[1]=0x%2X,tmp_addr[2]=0x%2X,tmp_addr[3]=0x%2X\n",temp_addr,tmp_addr[0],tmp_addr[1],tmp_addr[2],tmp_addr[3]);
		tmp_addr[0] = temp_addr % 0x100;
		tmp_addr[1] = (temp_addr >> 8) % 0x100;
		tmp_addr[2] = (temp_addr >> 16) % 0x100;
		tmp_addr[3] = temp_addr / 0x1000000;
		hx83102_register_read(hx83102_ts->client, tmp_addr ,128,&flash_tmp_buffer[temp_addr - start_addr],false);
		// memcpy(&flash_tmp_buffer[temp_addr - start_addr],buffer,128);
	}
}

int hx83102_read_project_id(void)
{
	// uint8_t tmp_addr[4];
	uint32_t panel_color_addr = 0x10000;/*64k*/
	
	uint32_t info_len;
	uint32_t flash_page_len = 0x1000;/*4k*/
	uint8_t *flash_tmp_buffer 	= NULL;
	// uint32_t temp_addr = 0;
	char temp_str[128];
	int i = 0;
	int custom_info_temp = 0;
	
	hx83102_guest_info_set_status(1);

	flash_tmp_buffer = kzalloc(HX_GUEST_INFO_SIZE * flash_page_len * sizeof(uint8_t), GFP_KERNEL);
	hx83102_sense_off(hx83102_ts->client);
	hx83102_burst_enable(hx83102_ts->client, 1);
	
	
	
	for(custom_info_temp = 0; custom_info_temp < HX_GUEST_INFO_SIZE;custom_info_temp++)
	{
		himax_guest_info_read(panel_color_addr+custom_info_temp*flash_page_len,&flash_tmp_buffer[custom_info_temp*flash_page_len]);
		
		info_len = flash_tmp_buffer[custom_info_temp*flash_page_len] 
		+ (flash_tmp_buffer[custom_info_temp*flash_page_len+1] << 8)
		+ (flash_tmp_buffer[custom_info_temp*flash_page_len+2] << 16)
		+ (flash_tmp_buffer[custom_info_temp*flash_page_len+3] << 24);
		I("Now custom_info_temp = %d,[0]=%d,[1]=%d,[2]=%d,[3]=%d,info_len=%d\n"
		,custom_info_temp
		,flash_tmp_buffer[custom_info_temp*flash_page_len]
		,flash_tmp_buffer[custom_info_temp*flash_page_len+1]
		,flash_tmp_buffer[custom_info_temp*flash_page_len+2]
		,flash_tmp_buffer[custom_info_temp*flash_page_len+3]
		,info_len);
		
		if(info_len > 128)
			info_len = 128;
		
		for(i = 0;i < info_len; i++)
			temp_str[i] = flash_tmp_buffer[custom_info_temp*flash_page_len+HX_GUEST_INFO_LEN_SIZE+i];
		
		I("g_guest_str[%d] size = %d\n",custom_info_temp,info_len);
		memcpy(&g_guest_str[custom_info_temp][0],temp_str,info_len);
		if(custom_info_temp == 0)
		{
			for( i = 0; i< 128 ; i++)
			{
				if(i % 16 == 0 && i > 0)
					I("\n");
				I("g_guest_str[%d][%d] = 0x%02X",custom_info_temp,i,g_guest_str[custom_info_temp][i]);
			}
		}
	}
	hx83102_burst_enable(hx83102_ts->client, 0);
	hx83102_sense_on(hx83102_ts->client, 0x01);
	

	kfree(flash_tmp_buffer);
	hx83102_guest_info_set_status(0);
	return NO_ERR;
}
#endif

#if defined(HX_SMART_WAKEUP)||defined(HX_HIGH_SENSE)||defined(HX_USB_DETECT_GLOBAL)
void hx83102_resend_cmd_func(bool suspended)
{
    struct hx_83102_ts_data *ts;

    ts = hx83102_ts;

#ifdef HX_SMART_WAKEUP
    hx83102_set_SMWP_enable(ts->client,ts->SMWP_enable,suspended);
#endif
#ifdef HX_HIGH_SENSE
    hx83102_set_HSEN_enable(ts->client,ts->HSEN_enable,suspended);
#endif
#ifdef HX_USB_DETECT_GLOBAL
    hx83102_cable_detect_func(true);
#endif
}

void hx83102_rst_cmd_recovery_func(bool suspended)
{

}
#endif

void hx83102_resume_ic_action(struct i2c_client *client)
{
    return;
}

void hx83102_suspend_ic_action(struct i2c_client *client)
{
    return;
}
