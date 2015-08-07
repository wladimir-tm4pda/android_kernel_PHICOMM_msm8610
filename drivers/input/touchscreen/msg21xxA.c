/* =======================================================================
 *
 * when        	who         	why                           		comment tag
 *
 * ----------	---------	-------------------------------------	--------------------------
 * 2014-03-14	guofeizhi	make MSG2138A compitable with Goodix	FEIXUN_TP_COMPATIBILITY_GUOFEIZHI_001
 * 2014-05-08	guofeizhi	make MSG2138A compitable with Goodix	FEIXUN_TP_COMPATIBILITY_GUOFEIZHI_001
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/cdev.h>
#include <linux/earlysuspend.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/regulator/consumer.h>

#include <asm/unistd.h>
#include <asm/uaccess.h>

#ifdef CONFIG_DEVICE_VERSION
#include <mach/fdv.h>
#endif

#if defined(CONFIG_PHICOMM_NOTIFIER)
#include <linux/phicomm_notifier.h>
#endif

#define TPD_DEVICE_MSG "msg21XXA"

#define U8         unsigned char
#define U16        unsigned short
#define U32        unsigned int
#define S8         signed char
#define S16        signed short
#define S32        signed int

#define u8         unsigned char
#define u16        unsigned short
#define u32        unsigned int
#define s8         signed char
#define s16        signed short
#define s32        signed int

#if 1
#define TPD_DEBUG(a,arg...) printk("msg" ": " a,##arg)
#else
#define TPD_DEBUG(arg...) 
#endif

//#define SWAP_X_Y   (1)
//#define REVERSE_X  (1)
//#define REVERSE_Y  (1)
#define LCD_WIDTH               (480)
#define LCD_HEIGHT              (854)
#define TPD_WIDTH               (2048)
#define TPD_HEIGHT              (2048)
#define TPD_OK                  (0)
#define MAX_TOUCH_NUM           (2)     
#define REPORT_PACKET_LENGTH    (8)
#define FW_ADDR_MSG_TP      (0x4C>>1)
#define FW_ADDR_MSG         (0xC4>>1)

#if defined(CONFIG_PHICOMM_NOTIFIER)
struct notifier_block msg_bl_nb;
#endif 
static 	struct input_dev *input=NULL;
struct i2c_client *tpd_i2c_client = NULL;
struct regulator *msg_vcc_i2c;
struct regulator *msg_vdd_i2c;

static struct work_struct msg_wq;

//#ifdef CONFIG_HAS_EARLYSUSPEND
//static struct early_suspend early_suspend;
//#endif

static char* fw_version = NULL;
static unsigned short curr_ic_major=0;
static unsigned short curr_ic_minor=0;

static unsigned int MSG_RESET_GPIO = 0;
static unsigned int MSG_INT_GPIO = 1;

#define MAX_KEY_NUM    (3)
const int tpd_key_array[MAX_KEY_NUM] = { KEY_MENU, KEY_HOMEPAGE, KEY_BACK};
//#define REPORT_KEY_WITH_COORD
#ifdef REPORT_KEY_WITH_COORD
#define BUTTON_W (100)
#define BUTTON_H (100)
static int tpd_keys_dim_local[MAX_KEY_NUM][4] = {{BUTTON_W/2*1,LCD_HEIGHT+BUTTON_H/2,BUTTON_W,BUTTON_H},{BUTTON_W/2*3,LCD_HEIGHT+BUTTON_H/2,BUTTON_W,BUTTON_H},{BUTTON_W/2*5,LCD_HEIGHT+BUTTON_H/2,BUTTON_W,BUTTON_H},{BUTTON_W/2*7,LCD_HEIGHT+BUTTON_H/2,BUTTON_W,BUTTON_H}};
#endif

#define FIRMWARE_UPDATE
#ifdef FIRMWARE_UPDATE
static u8 bFwUpdating = 0;
static void _msg_create_file_for_fwUpdate(void);

//#define AUTO_FIRMWARE_UPDATE
#ifdef AUTO_FIRMWARE_UPDATE
static void _msg_auto_updateFirmware(void);
#endif

#define PROC_FIRMWARE_UPDATE
#ifdef PROC_FIRMWARE_UPDATE
static void _msg_create_file_for_fwUpdate_proc(void);
#endif
#endif

//#define  PROXIMITY_WITH_TP
#ifdef PROXIMITY_WITH_TP
static u8 bEnableTpProximity = 0;
static u8 bFaceClosingTp = 0;
static void _msg_create_file_for_proximity_proc(void);
#endif

//#define ITO_TEST
#ifdef ITO_TEST
static u8 bItoTesting = 0;
static void ito_test_create_entry(void);
#endif

#if defined(FIRMWARE_UPDATE)||defined(PROXIMITY_WITH_TP)||defined(ITO_TEST)
static u8 bNeedResumeTp = 0;
#endif

typedef struct 
{
    U16 x;
    U16 y;
}   touchPoint_t;

typedef struct
{
    touchPoint_t point[MAX_TOUCH_NUM];
    U8 count;
    U8 keycode;
    U8 bKey;
    U8 bPoint;
    U8 bUp;
}   touchInfo_t;

static void _msg_disable_irq(void);
static void _msg_enable_irq(void);
static void _msg_resetHW(void);
static int _msg_ReadI2CSeq(U8 addr, U8* read_data, U16 size)
{
    int rc;
    U8 addr_before = tpd_i2c_client->addr;
    tpd_i2c_client->addr = addr;

    rc = i2c_master_recv(tpd_i2c_client, read_data, size);

    tpd_i2c_client->addr = addr_before;
    if( rc < 0 )
    {
        TPD_DEBUG("_msg_ReadI2CSeq error %d,addr=%d\n", rc,addr);
    }
    return rc;
}

static int _msg_WriteI2CSeq(U8 addr, U8* data, U16 size)
{
    int rc;
    U8 addr_before = tpd_i2c_client->addr;
    tpd_i2c_client->addr = addr;

    rc = i2c_master_send(tpd_i2c_client, data, size);

    tpd_i2c_client->addr = addr_before;
    if( rc < 0 )
    {
        TPD_DEBUG("_msg_WriteI2CSeq error %d,addr = %d,data[0]=%d\n", rc, addr,data[0]);
    }
    return rc;
}

static void _msg_WriteReg8Bit( U8 bank, U8 addr, U8 data )
{
    U8 tx_data[4] = {0x10, bank, addr, data};
    _msg_WriteI2CSeq ( FW_ADDR_MSG, &tx_data[0], 4 );
}

static void _msg_WriteReg( U8 bank, U8 addr, U16 data )
{
    U8 tx_data[5] = {0x10, bank, addr, data & 0xFF, data >> 8};
    _msg_WriteI2CSeq ( FW_ADDR_MSG, &tx_data[0], 5 );
}

static unsigned short _msg_ReadReg( U8 bank, U8 addr )
{
    U8 tx_data[3] = {0x10, bank, addr};
    U8 rx_data[2] = {0};

    _msg_WriteI2CSeq ( FW_ADDR_MSG, &tx_data[0], 3 );
	_msg_ReadI2CSeq ( FW_ADDR_MSG, &rx_data[0], 2 );
    return ( rx_data[1] << 8 | rx_data[0] );
}

static void _msg_EnterSerialDebugMode(void)
{
    U8 data[5];

    /// change mode
    data[0] = 0x53;
    data[1] = 0x45;
    data[2] = 0x52;
    data[3] = 0x44;
    data[4] = 0x42;
    _msg_WriteI2CSeq(FW_ADDR_MSG, &data[0], 5);

    /// stop mcu
    data[0] = 0x37;
    _msg_WriteI2CSeq(FW_ADDR_MSG, &data[0], 1);

    /// IIC use bus
    data[0] = 0x35;
    _msg_WriteI2CSeq(FW_ADDR_MSG, &data[0], 1);

    /// IIC reshape
    data[0] = 0x71;
    _msg_WriteI2CSeq(FW_ADDR_MSG, &data[0], 1);
}

static void _msg_ExitSerialDebugMode(void)
{
    U8 data[1];

    /// IIC not use bus
    data[0] = 0x34;
    _msg_WriteI2CSeq(FW_ADDR_MSG, &data[0], 1);

    /// not stop mcu
    data[0] = 0x36;
    _msg_WriteI2CSeq(FW_ADDR_MSG, &data[0], 1);

    /// change mode
    data[0] = 0x45;
    _msg_WriteI2CSeq(FW_ADDR_MSG, &data[0], 1);
}

static void _msg_GetVersion(void)
{
    U8 tx_data[3] = {0};
    U8 rx_data[4] = {0};
    U16 Major = 0, Minor = 0;
    int rc_w = 0, rc_r = 0;

    TPD_DEBUG("_msg_GetVersion\n");

    tx_data[0] = 0x53;
    tx_data[1] = 0x00;
    tx_data[2] = 0x2A;
    rc_w = _msg_WriteI2CSeq(FW_ADDR_MSG_TP, &tx_data[0], 3);
    TPD_DEBUG("***rc_w=%d ***\n", rc_w);
    mdelay(50);
    rc_r = _msg_ReadI2CSeq(FW_ADDR_MSG_TP, &rx_data[0], 4);
    TPD_DEBUG("***rc_r=%d ***\n", rc_r);
    mdelay(50);
    
    Major = (rx_data[1]<<8) + rx_data[0];
    Minor = (rx_data[3]<<8) + rx_data[2];
    TPD_DEBUG("***major = %d ***\n", Major);
    TPD_DEBUG("***minor = %d ***\n", Minor);

    if(rc_w<0||rc_r<0)
    {
        curr_ic_major = 0xffff;
        curr_ic_minor = 0xffff;
    }
    else
    {
        curr_ic_major = Major;
        curr_ic_minor = Minor;
    }
}

#ifdef AUTO_FIRMARE_UPDATA
#define TP_OF_XXX1 1
#define TP_OF_XXX2 2
#define TP_OF_XXX3 3
static u16 _msg_GetVersion_MoreTime(void)
{
    int version_check_time = 0;
    for(version_check_time=0;version_check_time<5;version_check_time++)
    {
        _msg_GetVersion();
        if(TP_OF_XXX1==curr_ic_major
           ||TP_OF_XXX2==curr_ic_major
           ||TP_OF_XXX3==curr_ic_major)
        {
            break;
        }
        else if(version_check_time<3)
        {
            mdelay(100);
        }
        else
        {
            _msg_resetHW();
        }
    }
    return curr_ic_major;
}
#endif
#if 1
static u8 _msg_GetIcType(void)
{
    u8 ic_type = 0;
   
	_msg_resetHW();
    _msg_EnterSerialDebugMode();
    mdelay ( 300 );
    
    // stop mcu
    _msg_WriteReg8Bit ( 0x0F, 0xE6, 0x01 );
    // disable watch dog
    _msg_WriteReg ( 0x3C, 0x60, 0xAA55 );
    // get ic type
    ic_type = (0xff)&(_msg_ReadReg(0x1E, 0xCC));
    TPD_DEBUG("_msg_GetIcType,ic_type=%d",ic_type);
    
    if(ic_type!=1//msg2133
        &&ic_type!=2//msg21xxA
        &&ic_type!=3)//msg26xx
    {
        ic_type = 0;
    }
    
    _msg_ExitSerialDebugMode();
    _msg_resetHW();
    
    return ic_type;
    
}
#endif

#ifdef REPORT_KEY_WITH_COORD
static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
			    __stringify(EV_KEY) ":" __stringify(tpd_key_array[0]) ":50 :904:100:100" //{BUTTON_W/2*1,LCD_HEIGHT+BUTTON_H/2,BUTTON_W,BUTTON_H}
			":" __stringify(EV_KEY) ":" __stringify(tpd_key_array[1]) ":200:904:100:100"//{BUTTON_W/2*3,LCD_HEIGHT+BUTTON_H/2,BUTTON_W,BUTTON_H}
			":" __stringify(EV_KEY) ":" __stringify(tpd_key_array[2]) ":350:904:100:100"//{BUTTON_W/2*5,LCD_HEIGHT+BUTTON_H/2,BUTTON_W,BUTTON_H}
			"\n");
}

static struct kobj_attribute virtual_keys_attr = {
	.attr = {
		.name = "virtualkeys.msg21XXA",
		.mode = S_IRUGO,
	},
	.show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
	&virtual_keys_attr.attr,
	NULL
};


static struct attribute_group properties_attr_group = {
	.attrs = properties_attrs,
};

static void msg_ts_virtual_keys_init(void)
{
	int ret;
	struct kobject *properties_kobj;

	TPD_DEBUG("%s\n",__func__);

	properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&properties_attr_group);
	if (!properties_kobj || ret)
		pr_err("failed to create board_properties\n");    
}
#endif

static U8 _msg_CalChecksum( U8 *msg, S32 s32Length )
{
    S32 s32Checksum = 0;
    S32 i;

    for ( i = 0 ; i < s32Length; i++ )
    {
        s32Checksum += msg[i];
    }

    return (U8)( ( -s32Checksum ) & 0xFF );
}

static S32 _msg_ParseInfo(touchInfo_t *info)
{
    U8 data[REPORT_PACKET_LENGTH] = {0};
    U8 checksum = 0;
    U32 x = 0, y = 0;
    U32 delta_x = 0, delta_y = 0;

    _msg_ReadI2CSeq( FW_ADDR_MSG_TP, &data[0], REPORT_PACKET_LENGTH );
    checksum = _msg_CalChecksum(&data[0], (REPORT_PACKET_LENGTH-1));
//  TPD_DEBUG("check sum: [%x] == [%x]? \n", data[REPORT_PACKET_LENGTH-1], checksum);    

    if(data[REPORT_PACKET_LENGTH-1] != checksum)
    {
        TPD_DEBUG("WRONG CHECKSUM\n"); 
        return -1;
    }

    if(data[0] != 0x52)
    {
        TPD_DEBUG("WRONG HEADER\n"); 
        return -1;
    }
    
    if( ( data[1] == 0xFF ) && ( data[2] == 0xFF ) && ( data[3] == 0xFF ) && ( data[4] == 0xFF ) && ( data[6] == 0xFF ) )
    {
        if(data[5]==0xFF||data[5]==0)
        {
            info->bUp = 1;
        }
        else if(data[5]==1||data[5]==2||data[5]==4||data[5]==8)
        {
        #ifdef REPORT_KEY_WITH_COORD
            info->bPoint = 1;
            info->count=1;
            if(data[5]==1)
            {
                info->point[0].x = tpd_keys_dim_local[0][0];
                info->point[0].y = tpd_keys_dim_local[0][1];
            }
            else if(data[5]==2)
            {
                info->point[0].x = tpd_keys_dim_local[1][0];
                info->point[0].y = tpd_keys_dim_local[1][1];
            }
            else if(data[5]==4)
            {
                info->point[0].x = tpd_keys_dim_local[2][0];
                info->point[0].y = tpd_keys_dim_local[2][1];
            }
            else if(data[5]==8)
            {
                info->point[0].x = tpd_keys_dim_local[3][0];
                info->point[0].y = tpd_keys_dim_local[3][1];
            }
        #else
            info->bKey = 1;
            if(data[5]==1)
            {
                info->keycode=0;
            }
            else if(data[5]==2)
            {
                info->keycode=1;
            }
            else if(data[5]==4)
            {
                info->keycode=2;
            }
            else if(data[5]==8)
            {
                info->keycode=3;
            }
        #endif
        }
    #ifdef PROXIMITY_WITH_TP
        else if(bEnableTpProximity
                &&(data[5]==0x80||data[5]==0x40))
        {
            if(data[5]==0x80&&!bFaceClosingTp)
            {
                bFaceClosingTp = 1;
            }
            else if(data[5]==0x40&&bFaceClosingTp)
            {
                bFaceClosingTp = 0;
            }
            TPD_DEBUG("bEnableTpProximity=%d;bFaceClosingTp=%d;data[5]=%x;\n",bEnableTpProximity,bFaceClosingTp,data[5]); 
            return -1;
        }
    #endif
        else
        {
            TPD_DEBUG("WRONG KEY\n"); 
            return -1;
        }
    }
    else
    {
        info->bPoint = 1;
        x = ( ( ( data[1] & 0xF0 ) << 4 ) | data[2] );   
        y = ( ( ( data[1] & 0x0F ) << 8 ) | data[3] );
        delta_x = ( ( ( data[4] & 0xF0 ) << 4 ) | data[5] );
        delta_y = ( ( ( data[4] & 0x0F ) << 8 ) | data[6] );
        
        if(delta_x==0&&delta_y==0)
        {
            info->point[0].x = x * LCD_WIDTH / TPD_WIDTH;
            info->point[0].y = y * LCD_HEIGHT/ TPD_HEIGHT;
            info->count=1;
        }
        else
        {
            u32 x2=0, y2=0;
            if( delta_x > 2048 )    
            {
                delta_x -= 4096;
            }
            if( delta_y > 2048 )
            {
                delta_y -= 4096;
            }
            x2 = ( u32 )( (s16)x + (s16)delta_x );
            y2 = ( u32 )( (s16)y + (s16)delta_y );
            info->point[0].x = x * LCD_WIDTH / TPD_WIDTH;
            info->point[0].y = y * LCD_HEIGHT/ TPD_HEIGHT;
            info->point[1].x = x2 * LCD_WIDTH / TPD_WIDTH;
            info->point[1].y = y2 * LCD_HEIGHT/ TPD_HEIGHT;
            info->count=2;
        }
    }
    return TPD_OK;
}

static void tpd_down(int x, int y) 
{
#ifdef SWAP_X_Y
    int temp;
    temp = x;
    x = y;
    y = temp;
#endif
#ifdef REVERSE_X
    x = LCD_WIDTH-x;
#endif
#ifdef REVERSE_Y
    y = LCD_HEIGHT-y;
#endif
    input_report_key(input, BTN_TOUCH, 1); 
    input_report_abs(input, ABS_MT_TOUCH_MAJOR, 1);
    input_report_abs(input, ABS_MT_POSITION_X, x);
    input_report_abs(input, ABS_MT_POSITION_Y, y);
    input_mt_sync(input);

}
 
static void tpd_up(void) 
{
    input_report_key(input, BTN_TOUCH, 0);
    input_mt_sync(input);
}


static irqreturn_t msg_interrupt(int irq, void *dev_id)//modify :??????Ҫ?޸ġ?
{	
//  TPD_DEBUG("msg_interrupt\n");
    _msg_disable_irq();
    schedule_work(&msg_wq);
    return IRQ_HANDLED;
}

static int last_keycode = 0;
static void msg_do_work(struct work_struct *work)
{
	touchInfo_t info;
	int i = 0;

//	TPD_DEBUG("msg_do_work \n");
    memset(&info, 0x0, sizeof(info));
    if(TPD_OK == _msg_ParseInfo(&info))
    {
        if (info.bKey)
        {     
//            TPD_DEBUG("msg_do_work  info.keycode =%x\n",info.keycode);
            last_keycode = info.keycode;
            //input_report_key(input, BTN_TOUCH, 1);
            input_report_key(input, tpd_key_array[info.keycode], 1);
        }
        else if(info.bPoint)          
        {
//            TPD_DEBUG("msg_do_work  point....\n");
            last_keycode = -1;
            for(i=0; i<info.count; i++)
            {
                tpd_down(info.point[i].x, info.point[i].y);
//                TPD_DEBUG("msg_do_work: point[%d](%03d, %03d).\n", i, info.point[i].x, info.point[i].y);        
            }           
        }
        else if(info.bUp)                        
        {
            if(last_keycode == -1)
            {
            	tpd_up();        
            }
            else
            {
//              TPD_DEBUG("msg_do_work  up...info.keycode =%x\n",info.keycode);
            	input_report_key(input, tpd_key_array[last_keycode], 0);
            	//tpd_up();        
            }
        } 
        input_sync(input);            
    }
    
	_msg_enable_irq();
}


static void _msg_init_input(void)
{
	int err;

	TPD_DEBUG("%s: msg21xx_i2c_client->name:%s\n", __func__,tpd_i2c_client->name);
	input = input_allocate_device();
//	input->name = tpd_i2c_client->name;
//	input->phys = "I2C";
//	input->id.bustype = BUS_I2C;
//	input->dev.parent = &tpd_i2c_client->dev;


	set_bit(EV_ABS, input->evbit);
	set_bit(EV_SYN, input->evbit);
	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_TOUCH, input->keybit);   
	set_bit(INPUT_PROP_DIRECT, input->propbit);
//	set_bit(BTN_MISC,input->keybit);
//      set_bit(KEY_OK, input->keybit);	

    {
        int i;
        for(i = 0; i < MAX_KEY_NUM; i++)
        {   
		input_set_capability(input, EV_KEY, tpd_key_array[i]);			
        }
    }

	input_set_abs_params(input, ABS_MT_POSITION_X, 0, LCD_WIDTH, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y, 0, LCD_HEIGHT, 0, 0);
	input_set_abs_params(input, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	err = input_register_device(input);
	TPD_DEBUG("error = %d\n", err);
}
static int _msg_ts_power_init(struct i2c_client *client)
{
    msg_vcc_i2c = regulator_get(&(client->dev), "vcc-i2c");
    TPD_DEBUG("msg msg_vcc_i2c = %#08x,%#08x\n", (unsigned)msg_vcc_i2c,(unsigned)&(client->dev));
    if(NULL == msg_vcc_i2c){
        goto VDD_GET;
    }
    regulator_set_voltage(msg_vcc_i2c, 1800000, 1800000);

VDD_GET:
    msg_vdd_i2c = regulator_get(&(client->dev), "vdd");
    TPD_DEBUG("msg vdd = %#08x,%#08x\n", (unsigned)msg_vcc_i2c,(unsigned)&(client->dev));
    if(NULL == msg_vdd_i2c){
	return -ENODEV;
    }
    regulator_set_voltage(msg_vdd_i2c, 2850000, 2850000);

    return 0;
}

static void _msg_ts_power_on(struct i2c_client *client, int on)
{
    if(!!on){
	if(msg_vcc_i2c){
	    regulator_enable(msg_vcc_i2c);
	}
	if(msg_vdd_i2c){
	    regulator_enable(msg_vdd_i2c);
	}
    }else{
	if(msg_vcc_i2c){
	   regulator_disable(msg_vcc_i2c);
        }
	if(msg_vdd_i2c){
	   regulator_disable(msg_vdd_i2c);
	}
    }	
}

static void _msg_init_vdd(struct i2c_client *client)
{
   _msg_ts_power_init(client);
   _msg_ts_power_on(client,1);
}

static void _msg_init_rst(void)
{
	int err=0;
	MSG_RESET_GPIO = 0;
	gpio_tlmm_config(GPIO_CFG(MSG_RESET_GPIO, 0, GPIO_CFG_OUTPUT,GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	err = gpio_request(0, "msg_reset_gpio");
	if (err)
	{
            TPD_DEBUG("MSG reset gpio request failed\n");
	}
	return ;	 
}

static void _msg_set_rst_high(void)

{  
     gpio_direction_output(MSG_RESET_GPIO, 1);
}

static void _msg_set_rst_low(void)
{  
    gpio_direction_input(MSG_RESET_GPIO);
    msleep(50);
    gpio_direction_output(MSG_RESET_GPIO, 0);
}

static void _msg_resetHW(void)
{  
    _msg_set_rst_low();
    mdelay( 10 ); 
    _msg_set_rst_high();
    mdelay( 300 );
}

static void _msg_init_irq(void)
{  
	int err=0;
	gpio_tlmm_config(GPIO_CFG(MSG_INT_GPIO, 0, GPIO_CFG_INPUT,GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	MSG_INT_GPIO = gpio_to_irq(1);
	err = gpio_request(1, "msg_irq_gpio");
	if (err)
	{
		TPD_DEBUG("MSG irq gpio request failed %d\n",err);
	}
        err = request_irq(MSG_INT_GPIO, msg_interrupt, IRQF_TRIGGER_FALLING, "msg_irq", NULL);
   	TPD_DEBUG("irq err %d",err); 
}
static void _msg_disable_irq(void)
{  
     disable_irq_nosync(MSG_INT_GPIO);
}
static void _msg_enable_irq(void)
{  
    enable_irq(MSG_INT_GPIO);
}

static int tpd_resume(struct i2c_client *client)
{
#if defined(FIRMWARE_UPDATE)||defined(PROXIMITY_WITH_TP)||defined(ITO_TEST)
    if(bNeedResumeTp)
    {
        TPD_DEBUG("TPD wake up\n");
        _msg_set_rst_high();
        mdelay(100);
        _msg_enable_irq();
        TPD_DEBUG("TPD wake up done\n");
    }
    else
    {
        TPD_DEBUG("no need resume tp\n");
    }
#else
    TPD_DEBUG("TPD wake up\n");
    _msg_set_rst_high();
    mdelay(100);
    _msg_enable_irq();
    TPD_DEBUG("TPD wake up done\n");
#endif

	return 0;
}

static int tpd_suspend(struct i2c_client *client, pm_message_t mesg)
{
    TPD_DEBUG("TPD enter sleep\n");
#if defined(FIRMWARE_UPDATE)||defined(PROXIMITY_WITH_TP)||defined(ITO_TEST)
    bNeedResumeTp = 0;
#endif
#ifdef PROXIMITY_WITH_TP
    if(bEnableTpProximity)
    {
        TPD_DEBUG("TPD canot enter sleep bEnableTpProximity=%d\n",bEnableTpProximity);
        return 0;
    }
#endif
#ifdef FIRMWARE_UPDATE
    if(bFwUpdating)
    {
        TPD_DEBUG("TPD canot enter sleep bFwUpdating=%d\n",bFwUpdating);
        return 0;
    }
#endif
#ifdef ITO_TEST
    if(bItoTesting)
    {
        {
            TPD_DEBUG("TPD canot enter sleep bItoTesting=%d\n",bItoTesting);
            return 0;
        }
    }
#endif
    _msg_disable_irq();
    _msg_set_rst_low();
#if defined(FIRMWARE_UPDATE)||defined(PROXIMITY_WITH_TP)||defined(ITO_TEST)
    bNeedResumeTp = 1;
#endif
    TPD_DEBUG("TPD enter sleep done\n");

	return 0;
}

#if defined(CONFIG_PHICOMM_NOTIFIER)
static int tpd_resume_1(struct i2c_client *client)
{
    TPD_DEBUG("TPD wake up\n");
    _msg_resetHW();
    _msg_enable_irq();
    TPD_DEBUG("TPD wake up done\n");

    return 0;
}

static int tpd_suspend_1(struct i2c_client *client)
{
    TPD_DEBUG("TPD enter sleep\n");
    _msg_disable_irq();
    _msg_set_rst_low();
    TPD_DEBUG("TPD enter sleep done\n");
    return 0;
}

static int bl_notifier(struct notifier_block *self, unsigned long val, void *v)
{
    printk(KERN_DEBUG "***************bl_nb_val=%d\n", (int)val);

    if(!!val == BL_ON){
	tpd_resume_1(tpd_i2c_client);
    }else if(!!val == BL_OFF){
	tpd_suspend_1(tpd_i2c_client);
    }

    return NOTIFY_OK;
}
#endif
//FEIXUN_TP_COMPATIBILITY_GUOFEIZHI_001 start
#if defined (CONFIG_PHICOMM_BOARD_C230WEU) || defined (CONFIG_PHICOMM_BOARD_E550W)
extern unsigned TP_EXIST_FLAG;
#endif
//FEIXUN_TP_COMPATIBILITY_GUOFEIZHI_001 end
static int tpd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{     
    TPD_DEBUG("TPD probe:%#08x\n",(unsigned)client);
//FEIXUN_TP_COMPATIBILITY_GUOFEIZHI_001 start
#if defined (CONFIG_PHICOMM_BOARD_C230WEU) || defined (CONFIG_PHICOMM_BOARD_E550W)
    if(!!TP_EXIST_FLAG){
        printk("MSG21xxA: TP_EXIST_FLAG = %d, other TP already registered.\n", TP_EXIST_FLAG);
        return -1;
    }
#endif
//FEIXUN_TP_COMPATIBILITY_GUOFEIZHI_001 end
    tpd_i2c_client = client;
    _msg_init_vdd(tpd_i2c_client);
    _msg_init_rst();
    _msg_resetHW();
#if 1
    if(0==_msg_GetIcType())
    {
	gpio_free(0);
        TPD_DEBUG("The currnet ic is not MSG\n");
        printk("MSG21xxA: MSG TP is not connected.\n");
        return -1;
    }
#endif   
    _msg_init_input();
    _msg_init_irq();
    _msg_disable_irq();
    INIT_WORK(&msg_wq, msg_do_work);
    msleep(10);

#ifdef PROXIMITY_WITH_TP
    _msg_create_file_for_proximity_proc();
#endif
#ifdef ITO_TEST
    ito_test_create_entry();
#endif
#ifdef FIRMWARE_UPDATE
    _msg_create_file_for_fwUpdate();
#ifdef PROC_FIRMWARE_UPDATE
    _msg_create_file_for_fwUpdate_proc();
#endif
#ifdef AUTO_FIRMWARE_UPDATE
    _msg_auto_updateFirmware();
#endif
#endif
#ifdef REPORT_KEY_WITH_COORD
	msg_ts_virtual_keys_init();
#endif

//#ifdef CONFIG_HAS_EARLYSUSPEND
//      early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
//	early_suspend.suspend = tpd_suspend;
//	early_suspend.resume = tpd_resume;
//	register_early_suspend(&early_suspend);
//#endif

    _msg_enable_irq();
    TPD_DEBUG("TPD probe done\n");

#if defined(CONFIG_PHICOMM_NOTIFIER)
    msg_bl_nb.notifier_call = bl_notifier;
    msg_bl_nb.priority = 0;
    bl_notifier_chain_register(&msg_bl_nb);
#endif
//FEIXUN_TP_COMPATIBILITY_GUOFEIZHI_001 start
#if defined (CONFIG_PHICOMM_BOARD_C230WEU) || defined (CONFIG_PHICOMM_BOARD_E550W)
    confirm_fdv(DEV_TP,"Junda",MANUF_GOODIX_ID_JUNDA|0x26);
    TP_EXIST_FLAG = 1;
    printk("MSG2138A: TP already registered.\n");
#endif
//FEIXUN_TP_COMPATIBILITY_GUOFEIZHI_001 end
    return TPD_OK;   
}

static int __devexit tpd_remove(struct i2c_client *client)
{   
    TPD_DEBUG("TPD removed\n");
#if defined(CONFIG_PHICOMM_NOTIFIER)
    bl_notifier_chain_unregister(&msg_bl_nb);
#endif
	regulator_disable(msg_vcc_i2c);
	regulator_put(msg_vcc_i2c);
    return TPD_OK;
}

static const struct i2c_device_id tpd_id[] = {
	{ TPD_DEVICE_MSG, 0 },
	{ }
};

static struct i2c_driver tpd_i2c_driver =
{
    .driver = {
        .name = TPD_DEVICE_MSG,
    },
    .probe = tpd_probe,
    .remove = __devexit_p(tpd_remove),
    .suspend = tpd_suspend,
    .resume = tpd_resume,
    .id_table = tpd_id,
};

static void _msg_register_device_and_driver(void)
{
	i2c_add_driver(&tpd_i2c_driver);
}
static void _msg_del_device_and_driver(void)
{
    i2c_del_driver(&tpd_i2c_driver);
}
static int __init _msg_ts_init(void)
{
	TPD_DEBUG( "%s\n", __func__);
#if defined (CONFIG_PHICOMM_BOARD_E550W) || defined (CONFIG_PHICOMM_BOARD_C230WEU)
	register_fdv_with_desc(DEV_TP, "Junda", MANUF_GOODIX_ID_UNIRETE|0x26, "MSG2138A");
#endif
    _msg_register_device_and_driver();
	return 0;
}

static void __exit _msg_ts_exit(void)
{
    TPD_DEBUG( "%s\n", __func__);
	_msg_del_device_and_driver();
}

module_init(_msg_ts_init);
module_exit(_msg_ts_exit);

#ifdef FIRMWARE_UPDATE
#if 1
#define TPD_DEBUG_UPDATE(a,arg...) printk("msg_update" ": " a,##arg)
#else
#define TPD_DEBUG_UPDATE(arg...) 
#endif

#define FW_ADDR_MSG_UPDATE (0x4c>>1)
#define CTP_AUTHORITY 0777
static U8 temp[33][1024];
static U32 crc32_table[256];
static S32 FwDataCnt;
struct class *firmware_class;
struct device *firmware_cmd_dev;

/// CRC
static U32 _CRC_doReflect( U32 ref, S8 ch ) 
{
    U32 value = 0;
    U32 i = 0;

    for ( i = 1; i < ( ch + 1 ); i++ )
    {
        if ( ref & 1 )
        {
            value |= 1 << ( ch - i );
        }
        ref >>= 1;
    }
    
    return value;
}

U32 _CRC_getValue( U32 text, U32 prevCRC )
{
    U32 ulCRC = prevCRC;
    
    ulCRC = ( ulCRC >> 8 ) ^ crc32_table[ ( ulCRC & 0xFF ) ^ text];

    return ulCRC ;
}

static void _CRC_initTable( void )
{
    U32 magicnumber = 0x04c11db7;
    U32 i, j;

    for ( i = 0; i <= 0xFF; i++ )
    {
        crc32_table[i] = _CRC_doReflect ( i, 8 ) << 24;
        for ( j = 0; j < 8; j++ )
        {
            crc32_table[i] = ( crc32_table[i] << 1 ) ^ ( crc32_table[i] & ( 0x80000000L ) ? magicnumber : 0 );
        }
        crc32_table[i] = _CRC_doReflect ( crc32_table[i], 32 );
    }
}
typedef enum
{
    EMEM_ALL = 0,
    EMEM_MAIN,
    EMEM_INFO,
} EMEM_TYPE_t;
static S32 _updateFirmware_cash ( EMEM_TYPE_t emem_type )
{
    U32 i, j;
    U32 crc_main, crc_main_tp;
    U32 crc_info, crc_info_tp;
    U16 reg_data = 0;

    crc_main = 0xffffffff;
    crc_info = 0xffffffff;

    /////////////////////////
    // Erase
    /////////////////////////

    TPD_DEBUG_UPDATE("erase 0\n");
    _msg_resetHW();
    _msg_EnterSerialDebugMode();
    mdelay ( 300 );

    TPD_DEBUG_UPDATE ("erase 1\n");
        
    // stop mcu
    _msg_WriteReg8Bit ( 0x0F, 0xE6, 0x01 );

    // disable watch dog
    _msg_WriteReg ( 0x3C, 0x60, 0xAA55 );
    TPD_DEBUG_UPDATE("erase 2\n");
    // set PROGRAM password
    _msg_WriteReg ( 0x16, 0x1A, 0xABBA );
             
    // clear pce
    _msg_WriteReg8Bit ( 0x16, 0x18, 0x80 );
    mdelay ( 10 );
  
    TPD_DEBUG_UPDATE ("erase 3\n");  
    // trigger erase 
    if ( emem_type == EMEM_ALL )
    {
        _msg_WriteReg8Bit ( 0x16, 0x0E, 0x08 ); //all 
    }
    else
    {
        _msg_WriteReg8Bit ( 0x16, 0x0E, 0x04 ); //main
    }
     mdelay ( 10 );
    TPD_DEBUG_UPDATE("erase 4\n");
    do {
        reg_data = _msg_ReadReg ( 0x16, 0x10 );
    } while ( (reg_data & 0x0002) != 0x0002 );
    TPD_DEBUG_UPDATE("erase 5\n");
    // clear pce
    _msg_WriteReg8Bit ( 0x16, 0x18, 0x80 );
    mdelay ( 10 );     
    _msg_WriteReg ( 0x16, 0x00, 0x0000 );     
    _msg_WriteReg ( 0x16, 0x1A, 0x0000 ); 
    TPD_DEBUG_UPDATE("erase 6\n");
    //set pce to high
    _msg_WriteReg8Bit ( 0x16, 0x18, 0x40 );
    mdelay ( 10 );    
    TPD_DEBUG_UPDATE("erase 7\n");
    //wait pce to ready
    do {
      reg_data = _msg_ReadReg ( 0x16, 0x10 );
    } while ( (reg_data & 0x0004) != 0x0004 );   
    
    TPD_DEBUG_UPDATE ("erase OK\n");    
        
    /////////////////////////
    // Program
    /////////////////////////

        TPD_DEBUG_UPDATE ("program 0\n"); 
        
    _msg_resetHW();//must
    _msg_EnterSerialDebugMode();
    mdelay ( 300 );

        TPD_DEBUG_UPDATE ("program 1\n"); 

    // Check_Loader_Ready: polling 0x3CE4 is 0x1C70
    do {
        reg_data = _msg_ReadReg ( 0x3C, 0xE4 );
    } while ( reg_data != 0x1C70 );

        TPD_DEBUG_UPDATE ("program 2\n"); 
    if ( emem_type == EMEM_ALL )
    {
        _msg_WriteReg ( 0x3C, 0xE4, 0xE38F ); //all 
    }
    else
    {
        _msg_WriteReg ( 0x3C, 0xE4, 0x7731 );  //main
    }
       
      
	TPD_DEBUG_UPDATE ("program 11\n"); 
    mdelay ( 100 );
    
    // Check_Loader_Ready2Program: polling 0x3CE4 is 0x2F43
    do {
        reg_data = _msg_ReadReg ( 0x3C, 0xE4 );
    } while ( reg_data != 0x2F43 );

        TPD_DEBUG_UPDATE ("program 3\n"); 

    // prepare CRC & send data
    _CRC_initTable ();

    for ( i = 0; i < (32+1); i++ ) // main 32 KB  + info 1KB
    {
        if ( i == 32 )
        {
            for ( j = 0; j < 1024; j++ )
            {   
                crc_info = _CRC_getValue ( temp[i][j], crc_info);
            }
        }
        else if ( i < 31 )
        {
            for ( j = 0; j < 1024; j++ )
            {
                crc_main = _CRC_getValue ( temp[i][j], crc_main);
            }
        }        
        else if ( i == 31 )
        {
            temp[i][1014] = 0x5A; 
            temp[i][1015] = 0xA5; 

            for ( j = 0; j < 1016; j++ )
            {
                crc_main = _CRC_getValue ( temp[i][j], crc_main);
            }
        }
 
        for(j=0; j<8; j++)
        {
            TPD_DEBUG_UPDATE ("i=%d,j=%d\n",i,j); 
            _msg_WriteI2CSeq ( FW_ADDR_MSG_UPDATE, &temp[i][j*128], 128 );
        }
        mdelay ( 100 );
        
        // Check_Program_Done: polling 0x3CE4 is 0xD0BC
        do {
            reg_data = _msg_ReadReg ( 0x3C, 0xE4 );
        } while ( reg_data != 0xD0BC );
        
        // Continue_Program
        _msg_WriteReg ( 0x3C, 0xE4, 0x2F43 );
    }

        TPD_DEBUG_UPDATE ("program 4\n"); 

    // Notify_Write_Done
    _msg_WriteReg ( 0x3C, 0xE4, 0x1380 );
    mdelay ( 100 ); 

        TPD_DEBUG_UPDATE ("program 5\n"); 

    // Check_CRC_Done: polling 0x3CE4 is 0x9432
    do {
       reg_data = _msg_ReadReg ( 0x3C, 0xE4 );
    } while ( reg_data != 0x9432 );

        TPD_DEBUG_UPDATE ("program 6\n"); 

    // check CRC
    crc_main = crc_main ^ 0xffffffff;
    crc_info = crc_info ^ 0xffffffff;

    // read CRC from TP
    crc_main_tp = _msg_ReadReg ( 0x3C, 0x80 );
    crc_main_tp = ( crc_main_tp << 16 ) | _msg_ReadReg ( 0x3C, 0x82 );
    crc_info_tp = _msg_ReadReg ( 0x3C, 0xA0 );
    crc_info_tp = ( crc_info_tp << 16 ) | _msg_ReadReg ( 0x3C, 0xA2 );

    TPD_DEBUG ( "crc_main=0x%x, crc_info=0x%x, crc_main_tp=0x%x, crc_info_tp=0x%x\n",
               crc_main, crc_info, crc_main_tp, crc_info_tp );

    
    FwDataCnt = 0;
    _msg_ExitSerialDebugMode();
    _msg_resetHW();

    if ( crc_main_tp != crc_main )//all
    {
        TPD_DEBUG_UPDATE ( "update FAILED\n" );

        return -1;
    }

    TPD_DEBUG_UPDATE ( "update OK\n" );
    
    return TPD_OK;
}

////////////////////////////////////////////////////////////////////////////////
static ssize_t firmware_update_show ( struct device *dev,
                                      struct device_attribute *attr, char *buf )
{
    TPD_DEBUG_UPDATE("*** firmware_update_show fw_version = %s***\n", fw_version);

    return sprintf ( buf, "%s\n", fw_version );
}

static ssize_t firmware_update_store ( struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t size )
{
    bFwUpdating = 1;
	_msg_disable_irq();

    TPD_DEBUG_UPDATE("*** update fw size = %d ***\n", FwDataCnt);
    if( 0 != _updateFirmware_cash (EMEM_MAIN) )
    {
        size = 0;
        TPD_DEBUG_UPDATE ( "update failed\n" );        
    }
    else
    {
        TPD_DEBUG_UPDATE ( "update successfull\n" );
    }

	_msg_enable_irq();
    bFwUpdating = 0;
    return size;
}

static DEVICE_ATTR(update, CTP_AUTHORITY, firmware_update_show, firmware_update_store);

static ssize_t firmware_version_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    TPD_DEBUG_UPDATE("*** firmware_version_show fw_version = %s***\n", fw_version);
    
    return sprintf(buf, "%s\n", fw_version);
}

static ssize_t firmware_version_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t size)
{ 
    _msg_GetVersion();
    if(fw_version == NULL)
    {   
        fw_version = kzalloc(sizeof(*fw_version)*8, GFP_KERNEL);
    }
    sprintf(fw_version, "%03d%03d", curr_ic_major, curr_ic_minor);
    TPD_DEBUG_UPDATE("*** fw_version = %s ***\n", fw_version);
    if(buf != NULL)
        TPD_DEBUG_UPDATE("buf = %c ***\n", buf[0]);

    return size;
}

static DEVICE_ATTR(version, CTP_AUTHORITY, firmware_version_show, firmware_version_store);

static ssize_t firmware_data_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    TPD_DEBUG_UPDATE("*** firmware_data_show FwDataCnt = %d***\n", FwDataCnt);
    
    return sprintf(buf, "%d\n", FwDataCnt);
}

static ssize_t firmware_data_store(struct device *dev,
                                   struct device_attribute *attr, const char *buf, size_t size)
{
    int count = size / 1024;
    int i;

    for( i=0; i<count; i++ )
    {
        memcpy(temp[FwDataCnt], buf+(i*1024), 1024);
        FwDataCnt++;
    }

    TPD_DEBUG_UPDATE("*** FwDataCnt = %d ***\n", FwDataCnt);
    if(buf != NULL)
        TPD_DEBUG_UPDATE("buf = %c ***\n", buf[0]);
    
    return size;
}

static DEVICE_ATTR(data, CTP_AUTHORITY, firmware_data_show, firmware_data_store);

static void _msg_create_file_for_fwUpdate(void)
{  
    firmware_class = class_create(THIS_MODULE, "ms-touchscreen-msg20xx");
    if (IS_ERR(firmware_class))
        pr_err("Failed to create class(firmware)!\n");
    firmware_cmd_dev = device_create(firmware_class, NULL, 0, NULL, "device");
    if (IS_ERR(firmware_cmd_dev))
        pr_err("Failed to create device(firmware_cmd_dev)!\n");

    // version
    if (device_create_file(firmware_cmd_dev, &dev_attr_version) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_version.attr.name);
    // update
    if (device_create_file(firmware_cmd_dev, &dev_attr_update) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_update.attr.name);
    // data
    if (device_create_file(firmware_cmd_dev, &dev_attr_data) < 0)
        pr_err("Failed to create device file(%s)!\n", dev_attr_data.attr.name);

    dev_set_drvdata(firmware_cmd_dev, NULL);
}
#ifdef AUTO_FIRMWARE_UPDATE
static unsigned char tp_of_xxx1_update_bin[]=
{
    #include "tp_of_xxx1_update_bin.i"
};
static unsigned char tp_of_xxx2_update_bin[]=
{
    #include "tp_of_xxx2_update_bin.i"
};
static unsigned char tp_of_xxx3_update_bin[]=
{
    #include "tp_of_xxx3_update_bin.i"
};

static int _auto_updateFirmware_cash(void *unused)
{
    int update_time = 0;
    ssize_t ret = 0;
    for(update_time=0;update_time<5;update_time++)
    {
        TPD_DEBUG_UPDATE("update_time = %d\n",update_time);
        ret = firmware_update_store(NULL, NULL, NULL, 1);	
        if(ret==1)
        {
            TPD_DEBUG_UPDATE("AUTO_UPDATE OK!!!,update_time=%d\n",update_time);
            return 0;
        }
    }
    TPD_DEBUG_UPDATE("AUTO_UPDATE failed!!!,update_time=%d\n",update_time);
    return 0;
}

static void _msg_auto_updateFirmware(void)
{
    U16 tp_type = 0;
    unsigned short update_bin_major=0;
    unsigned short update_bin_minor=0;
    unsigned char *update_bin = NULL;

    tp_type=_msg_GetVersion_MoreTime();

    if(TP_OF_XXX1==tp_type)
    {
        update_bin = tp_of_xxx1_update_bin;
    }
    else if(TP_OF_XXX2==tp_type)
    {
        update_bin = tp_of_xxx2_update_bin;
    }
    else if(TP_OF_XXX3==tp_type)
    {
        update_bin = tp_of_xxx3_update_bin;
    }
    else
    {
        TPD_DEBUG_UPDATE("AUTO_UPDATE choose tp type failed,curr_ic_major=%d\n",tp_type);
        return;
    }
        
    
    FwDataCnt = 0;
    update_bin_major = update_bin[0x7f4f]<<8|update_bin[0x7f4e];
    update_bin_minor = update_bin[0x7f51]<<8|update_bin[0x7f50];
    TPD_DEBUG_UPDATE("bin_major = %d \n",update_bin_major);
    TPD_DEBUG_UPDATE("bin_minor = %d \n",update_bin_minor);
        
	if(update_bin_major==tp_type
        &&update_bin_minor>curr_ic_minor)
	{
	    int i = 0;
		for (i = 0; i < 33; i++)
		{
		    firmware_data_store(NULL, NULL, &(update_bin[i*1024]), 0);
		}
        kthread_run(_auto_updateFirmware_cash, 0, "MSG21XXA_fw_auto_update");
	}
    else 
    {
        TPD_DEBUG_UPDATE("AUTO_UPDATE not done,curr_ic_major=%d;curr_ic_minor=%d;update_bin_major=%d;update_bin_minor=%d\n",tp_type,curr_ic_minor,update_bin_major,update_bin_minor);
    }
}
#endif
#ifdef PROC_FIRMWARE_UPDATE
static int proc_version_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int cnt= 0;
    
    firmware_version_show(NULL,NULL, page);
    
    *eof = 1;
    return cnt;
}

static int proc_version_write(struct file *file, const char *buffer, unsigned long count, void *data)
{    
    firmware_version_store(NULL, NULL, NULL, 0);
    return count;
}

static int proc_update_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    count = (unsigned long)firmware_update_store(NULL, NULL, NULL, (size_t)count);	
    return count;
}

static int proc_data_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int cnt= 0;
    
    firmware_data_show(NULL, NULL, page);
    
    *eof = 1;    
    return cnt;
}

static int proc_data_write(struct file *file, const char *buffer, unsigned long count, void *data)
{
    firmware_data_store(NULL, NULL, buffer, 0);
    return count;
}
#define CTP_AUTHORITY_PROC 0777 
static void _msg_create_file_for_fwUpdate_proc(void)
{  
    struct proc_dir_entry *msg_class_proc = NULL;
    struct proc_dir_entry *msg_msg20xx_proc = NULL;
    struct proc_dir_entry *msg_device_proc = NULL;
    struct proc_dir_entry *msg_version_proc = NULL;
    struct proc_dir_entry *msg_update_proc = NULL;
    struct proc_dir_entry *msg_data_proc = NULL;
    
    msg_class_proc = proc_mkdir("class", NULL);
    msg_msg20xx_proc = proc_mkdir("ms-touchscreen-msg20xx",msg_class_proc);
    msg_device_proc = proc_mkdir("device",msg_msg20xx_proc);
    
    msg_version_proc = create_proc_entry("version", CTP_AUTHORITY_PROC, msg_device_proc);
    if (msg_version_proc == NULL) 
    {
        TPD_DEBUG_UPDATE("create_proc_entry msg_version_proc failed\n");
    } 
    else 
    {
        msg_version_proc->read_proc = proc_version_read;
        msg_version_proc->write_proc = proc_version_write;
        TPD_DEBUG_UPDATE("create_proc_entry msg_version_proc success\n");
    }
    msg_data_proc = create_proc_entry("data", CTP_AUTHORITY_PROC, msg_device_proc);
    if (msg_data_proc == NULL) 
    {
        TPD_DEBUG_UPDATE("create_proc_entry msg_data_proc failed\n");
    } 
    else 
    {
        msg_data_proc->read_proc = proc_data_read;
        msg_data_proc->write_proc = proc_data_write;
        TPD_DEBUG_UPDATE("create_proc_entry msg_data_proc success\n");
    }
    msg_update_proc = create_proc_entry("update", CTP_AUTHORITY_PROC, msg_device_proc);
    if (msg_update_proc == NULL) 
    {
        TPD_DEBUG_UPDATE("create_proc_entry msg_update_proc failed\n");
    } 
    else 
    {
        msg_update_proc->read_proc = NULL;
        msg_update_proc->write_proc = proc_update_write;
        TPD_DEBUG_UPDATE("create_proc_entry msg_update_proc success\n");
    }    
}
#endif
#endif  
#ifdef PROXIMITY_WITH_TP
static void _msg_enable_proximity(void)
{
    U8 tx_data[4] = {0};
    U8 rx_data[2] = {0};
    U8 i = 0;

    bEnableTpProximity = 0;
    bFaceClosingTp = 0;
    
    for(i=0;i<5;i++)
    {
        tx_data[0] = 0x52;
        tx_data[1] = 0x00;
        tx_data[2] = 0x4a;
        tx_data[3] = 0xa0;		
        _msg_WriteI2CSeq(FW_ADDR_MSG_TP, &tx_data[0], 4);
        mdelay(100);
        tx_data[0] = 0x53;
        tx_data[1] = 0x00;
        tx_data[2] = 0x4a;
        _msg_WriteI2CSeq(FW_ADDR_MSG_TP, &tx_data[0], 3);
        mdelay(50);
        _msg_ReadI2CSeq(FW_ADDR_MSG_TP, &rx_data[0], 2);
        mdelay(50);
        if(0xa0==rx_data[0])
        {
            bEnableTpProximity = 1;
            break;
        }
        else if(i<2)
        {
            mdelay(100);
        }
        else
        {
            _msg_resetHW();
        }
    }
    if(bEnableTpProximity)
    {
        TPD_DEBUG("Proximity enable success!!!");
    }
    else
    {
        TPD_DEBUG("Proximity enable failed!!!");
    }
}
static void _msg_disable_proximity(void)
{
    U8 tx_data[4] = {0};
    U8 rx_data[2] = {0};
    U8 i = 0;

    for(i=0;i<3;i++)
    {
        tx_data[0] = 0x52;
        tx_data[1] = 0x00;
        tx_data[2] = 0x4a;
        tx_data[3] = 0xa1;		
        _msg_WriteI2CSeq(FW_ADDR_MSG_TP, &tx_data[0], 4);
        mdelay(100);
        tx_data[0] = 0x53;
        tx_data[1] = 0x00;
        tx_data[2] = 0x4a;
        _msg_WriteI2CSeq(FW_ADDR_MSG_TP, &tx_data[0], 3);
        mdelay(50);
        _msg_ReadI2CSeq(FW_ADDR_MSG_TP, &rx_data[0], 2);
        mdelay(50);
        if(0xa1==rx_data[0])
        {
            break;
        }
        else if(i<2)
        {
            mdelay(100);
        }
        else
        {
            _msg_resetHW();
            break;
        }
    }
    bEnableTpProximity = 0;
}

static int proc_ps_read_to_debug(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int cnt= 0;
    
    U8 tx_data[3] = {0};
    U8 rx_data[2] = {0};
    TPD_DEBUG("bEnableTpProximity=%d;bFaceClosingTp=%d\n",bEnableTpProximity,bFaceClosingTp);
    tx_data[0] = 0x53;
    tx_data[1] = 0x00;
    tx_data[2] = 0x4a;
    _msg_WriteI2CSeq(FW_ADDR_MSG_TP, &tx_data[0], 3);
    mdelay(50);
    _msg_ReadI2CSeq(FW_ADDR_MSG_TP, &rx_data[0], 2);
    mdelay(50);
    TPD_DEBUG("rx_data[0]=%d\n",rx_data[0]);
    
    *eof = 1;    
    return cnt;
}

static int proc_ps_write_to_enable(struct file *file, const char *buffer, unsigned long count, void *data)
{
    int value = -1;

    if(NULL!=buffer)
    {
        sscanf(buffer,"%d",&value);
    }
    if(value==1)
    {
        _msg_enable_proximity();
        TPD_DEBUG("_msg_enable_proximity,bEnableTpProximity=%d",bEnableTpProximity);
    }
    else if(value==0)
    {
        _msg_disable_proximity();
        TPD_DEBUG("_msg_disable_proximity,bEnableTpProximity=%d",bEnableTpProximity);
    }
    else
    {
        TPD_DEBUG("proc_ps_write_to_enable wrong input");
    }
    return count;
}
static void _msg_create_file_for_proximity_proc(void)
{  
    struct proc_dir_entry *msg_tp_ps = NULL;
    struct proc_dir_entry *msg_ps_proc = NULL;

    msg_tp_ps = proc_mkdir("tp_ps", NULL);
    msg_ps_proc = create_proc_entry("ps", 0777, msg_tp_ps);

    if (msg_ps_proc == NULL) 
    {
        TPD_DEBUG("create_proc_entry tp_ps failed\n");
    } 
    else 
    {
        msg_ps_proc->read_proc = proc_ps_read_to_debug;
        msg_ps_proc->write_proc = proc_ps_write_to_enable;
        TPD_DEBUG("create_proc_entry tp_ps success\n");
    }
}
#endif
#ifdef ITO_TEST

#include <open_test_ANA1_XXX1.h>
#include <open_test_ANA2_XXX1.h>
#include <open_test_ANA1_B_XXX1.h>
#include <open_test_ANA2_B_XXX1.h>
#include <open_test_ANA3_XXX1.h>

#include <open_test_ANA1_XXX2.h>
#include <open_test_ANA2_XXX2.h>
#include <open_test_ANA1_B_XXX2.h>
#include <open_test_ANA2_B_XXX2.h>
#include <open_test_ANA3_XXX2.h>

#include <open_test_ANA1_XXX3.h>
#include <open_test_ANA2_XXX3.h>
#include <open_test_ANA1_B_XXX3.h>
#include <open_test_ANA2_B_XXX3.h>
#include <open_test_ANA3_XXX3.h>

///////////////////////////////////////////////////////////////////////////
u8 bItoTestDebug = 0;
#define ITO_TEST_DEBUG(format, ...) \
{ \
    if(bItoTestDebug) \
    { \
        printk(KERN_ERR "ito_test ***" format "\n", ## __VA_ARGS__); \
        mdelay(5); \
    } \
}
#define ITO_TEST_DEBUG_MUST(format, ...)	printk(KERN_ERR "ito_test ***" format "\n", ## __VA_ARGS__);mdelay(5)

static s16  s16_raw_data_1[48] = {0};
static s16  s16_raw_data_2[48] = {0};
static s16  s16_raw_data_3[48] = {0};
static u8 ito_test_keynum = 0;
static u8 ito_test_dummynum = 0;
static u8 ito_test_trianglenum = 0;
static u8 ito_test_2r = 0;
static u8 g_LTP = 1;	
static uint16_t *open_1 = NULL;
static uint16_t *open_1B = NULL;
static uint16_t *open_2 = NULL;
static uint16_t *open_2B = NULL;
static uint16_t *open_3 = NULL;
static u8 *MAP1=NULL;
static u8 *MAP2=NULL;
static u8 *MAP3=NULL;
static u8 *MAP40_1 = NULL;
static u8 *MAP40_2 = NULL;
static u8 *MAP40_3 = NULL;
static u8 *MAP40_4 = NULL;
static u8 *MAP41_1 = NULL;
static u8 *MAP41_2 = NULL;
static u8 *MAP41_3 = NULL;
static u8 *MAP41_4 = NULL;

#define REG_INTR_FIQ_MASK           0x04
#define FIQ_E_FRAME_READY_MASK      ( 1 << 8 )
#define MAX_CHNL_NUM (48)
#define BIT0  (1<<0)
#define BIT1  (1<<1)
#define BIT5  (1<<5)
#define BIT11 (1<<11)
#define BIT15 (1<<15)

static void ito_test_set_iic_rate(u32 iicRate)
{
	#ifdef CONFIG_I2C_SPRD
        sprd_i2c_ctl_chg_clk(msg21xx_i2c_client->adapter->nr, iicRate);
        mdelay(100);
	#endif
    #ifdef MTK
        msg21xx_i2c_client->timing = iicRate/1000;
    #endif
}

static u32 ito_test_choose_TpType(void)
{
    u16 tpType = 0;
    u8 i = 0;
    open_1 = NULL;
    open_1B = NULL;
    open_2 = NULL;
    open_2B = NULL;
    open_3 = NULL;
    MAP1 = NULL;
    MAP2 = NULL;
    MAP3 = NULL;
    MAP40_1 = NULL;
    MAP40_2 = NULL;
    MAP40_3 = NULL;
    MAP40_4 = NULL;
    MAP41_1 = NULL;
    MAP41_2 = NULL;
    MAP41_3 = NULL;
    MAP41_4 = NULL;
    ito_test_keynum = 0;
    ito_test_dummynum = 0;
    ito_test_trianglenum = 0;
    ito_test_2r = 0;

    tpType = _msg_GetVersion_MoreTime();

    if(TP_OF_XXX1==tpType)
    {
        open_1 = open_1_xxx1;
        open_1B = open_1B_xxx1;
        open_2 = open_2_xxx1;
        open_2B = open_2B_xxx1;
        open_3 = open_3_xxx1;
        MAP1 = MAP1_xxx1;
        MAP2 = MAP2_xxx1;
        MAP3 = MAP3_xxx1;
        MAP40_1 = MAP40_1_xxx1;
        MAP40_2 = MAP40_2_xxx1;
        MAP40_3 = MAP40_3_xxx1;
        MAP40_4 = MAP40_4_xxx1;
        MAP41_1 = MAP41_1_xxx1;
        MAP41_2 = MAP41_2_xxx1;
        MAP41_3 = MAP41_3_xxx1;
        MAP41_4 = MAP41_4_xxx1;
        ito_test_keynum = NUM_KEY_xxx1;
        ito_test_dummynum = NUM_DUMMY_xxx1;
        ito_test_trianglenum = NUM_SENSOR_xxx1;
        ito_test_2r = ENABLE_2R_xxx1;
    }
    else if(TP_OF_XXX2==tpType)
    {
        open_1 = open_1_xxx2;
        open_1B = open_1B_xxx2;
        open_2 = open_2_xxx2;
        open_2B = open_2B_xxx2;
        open_3 = open_3_xxx2;
        MAP1 = MAP1_xxx2;
        MAP2 = MAP2_xxx2;
        MAP3 = MAP3_xxx2;
        MAP40_1 = MAP40_1_xxx2;
        MAP40_2 = MAP40_2_xxx2;
        MAP40_3 = MAP40_3_xxx2;
        MAP40_4 = MAP40_4_xxx2;
        MAP41_1 = MAP41_1_xxx2;
        MAP41_2 = MAP41_2_xxx2;
        MAP41_3 = MAP41_3_xxx2;
        MAP41_4 = MAP41_4_xxx2;
        ito_test_keynum = NUM_KEY_xxx2;
        ito_test_dummynum = NUM_DUMMY_xxx2;
        ito_test_trianglenum = NUM_SENSOR_xxx2;
        ito_test_2r = ENABLE_2R_xxx2;
    }
    else if(TP_OF_XXX3==tpType)
    {
        open_1 = open_1_xxx3;
        open_1B = open_1B_xxx3;
        open_2 = open_2_xxx3;
        open_2B = open_2B_xxx3;
        open_3 = open_3_xxx3;
        MAP1 = MAP1_xxx3;
        MAP2 = MAP2_xxx3;
        MAP3 = MAP3_xxx3;
        MAP40_1 = MAP40_1_xxx3;
        MAP40_2 = MAP40_2_xxx3;
        MAP40_3 = MAP40_3_xxx3;
        MAP40_4 = MAP40_4_xxx3;
        MAP41_1 = MAP41_1_xxx3;
        MAP41_2 = MAP41_2_xxx3;
        MAP41_3 = MAP41_3_xxx3;
        MAP41_4 = MAP41_4_xxx3;
        ito_test_keynum = NUM_KEY_xxx3;
        ito_test_dummynum = NUM_DUMMY_xxx3;
        ito_test_trianglenum = NUM_SENSOR_xxx3;
        ito_test_2r = ENABLE_2R_xxx3;
    }
    else
    {
        tpType = 0;
    }
    return tpType;
}

static uint16_t ito_test_get_num( void )
{
    uint16_t    num_of_sensor,i;
    uint16_t 	RegValue1,RegValue2;
 
    num_of_sensor = 0;
        
    RegValue1 = _msg_ReadReg( 0x11, 0x4A);
    ITO_TEST_DEBUG("ito_test_get_num,RegValue1=%d\n",RegValue1);
    if ( ( RegValue1 & BIT1) == BIT1 )
    {
    	RegValue1 = _msg_ReadReg( 0x12, 0x0A);			
    	RegValue1 = RegValue1 & 0x0F;
    	
    	RegValue2 = _msg_ReadReg( 0x12, 0x16);    		
    	RegValue2 = (( RegValue2 >> 1 ) & 0x0F) + 1;
    	
    	num_of_sensor = RegValue1 * RegValue2;
    }
	else
	{
	    for(i=0;i<4;i++)
	    {
	        num_of_sensor+=(_msg_ReadReg( 0x12, 0x0A)>>(4*i))&0x0F;
	    }
	}
    ITO_TEST_DEBUG("ito_test_get_num,num_of_sensor=%d\n",num_of_sensor);
    return num_of_sensor;        
}
static void ito_test_polling( void )
{
    uint16_t    reg_int = 0x0000;
    uint8_t     dbbus_tx_data[5];
    uint8_t     dbbus_rx_data[4];
    uint16_t    reg_value;


    reg_int = 0;

    _msg_WriteReg( 0x13, 0x0C, BIT15 );       
    _msg_WriteReg( 0x12, 0x14, (_msg_ReadReg(0x12,0x14) | BIT0) );         
            
    ITO_TEST_DEBUG("polling start\n");
    while( ( reg_int & BIT0 ) == 0x0000 )
    {
        dbbus_tx_data[0] = 0x10;
        dbbus_tx_data[1] = 0x3D;
        dbbus_tx_data[2] = 0x18;
        _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, 3);
        _msg_ReadI2CSeq(FW_ADDR_MSG,  dbbus_rx_data, 2);
        reg_int = dbbus_rx_data[1];
    }
    ITO_TEST_DEBUG("polling end\n");
    reg_value = _msg_ReadReg( 0x3D, 0x18 ); 
    _msg_WriteReg( 0x3D, 0x18, reg_value & (~BIT0) );      
}
static uint16_t ito_test_get_data_out( int16_t* s16_raw_data )
{
    uint8_t     i,dbbus_tx_data[8];
    uint16_t    raw_data[48]={0};
    uint16_t    num_of_sensor;
    uint16_t    reg_int;
    uint8_t		dbbus_rx_data[96]={0};
  
    num_of_sensor = ito_test_get_num();
    if(num_of_sensor>11)
    {
        ITO_TEST_DEBUG("danger,num_of_sensor=%d\n",num_of_sensor);
        return num_of_sensor;
    }

    reg_int = _msg_ReadReg( 0x3d, REG_INTR_FIQ_MASK<<1 ); 
    _msg_WriteReg( 0x3d, REG_INTR_FIQ_MASK<<1, (reg_int & (uint16_t)(~FIQ_E_FRAME_READY_MASK) ) ); 
    ito_test_polling();
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x13;
    dbbus_tx_data[2] = 0x40;
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, 3);
    mdelay(20);
    _msg_ReadI2CSeq(FW_ADDR_MSG, &dbbus_rx_data[0], (num_of_sensor * 2));
    mdelay(100);
    for(i=0;i<num_of_sensor * 2;i++)
    {
        ITO_TEST_DEBUG("dbbus_rx_data[%d]=%d\n",i,dbbus_rx_data[i]);
    }
 
    reg_int = _msg_ReadReg( 0x3d, REG_INTR_FIQ_MASK<<1 ); 
    _msg_WriteReg( 0x3d, REG_INTR_FIQ_MASK<<1, (reg_int | (uint16_t)FIQ_E_FRAME_READY_MASK ) ); 


    for( i = 0; i < num_of_sensor; i++ )
    {
        raw_data[i] = ( dbbus_rx_data[ 2 * i + 1] << 8 ) | ( dbbus_rx_data[2 * i] );
        s16_raw_data[i] = ( int16_t )raw_data[i];
    }
    
    return(num_of_sensor);
}


static void ito_test_send_data_in( uint8_t step )
{
    uint16_t	i;
    uint8_t 	dbbus_tx_data[512];
    uint16_t 	*Type1=NULL;        

    ITO_TEST_DEBUG("ito_test_send_data_in step=%d\n",step);
	if( step == 4 )
    {
        Type1 = &open_1[0];        
    }
    else if( step == 5 )
    {
        Type1 = &open_2[0];      	
    }
    else if( step == 6 )
    {
        Type1 = &open_3[0];      	
    }
    else if( step == 9 )
    {
        Type1 = &open_1B[0];        
    }
    else if( step == 10 )
    {
        Type1 = &open_2B[0];      	
    } 
     
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;
    dbbus_tx_data[2] = 0x00;    
    for( i = 0; i <= 0x3E ; i++ )
    {
        dbbus_tx_data[3+2*i] = Type1[i] & 0xFF;
        dbbus_tx_data[4+2*i] = ( Type1[i] >> 8 ) & 0xFF;    	
    }
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, 3+0x3F*2);
 
    dbbus_tx_data[2] = 0x7A * 2;
    for( i = 0x7A; i <= 0x7D ; i++ )
    {
        dbbus_tx_data[3+2*(i-0x7A)] = 0;
        dbbus_tx_data[4+2*(i-0x7A)] = 0;    	    	
    }
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, 3+8);  
    
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x12;
      
    dbbus_tx_data[2] = 5 * 2;
    dbbus_tx_data[3] = Type1[128+5] & 0xFF;
    dbbus_tx_data[4] = ( Type1[128+5] >> 8 ) & 0xFF;
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, 5);
    
    dbbus_tx_data[2] = 0x0B * 2;
    dbbus_tx_data[3] = Type1[128+0x0B] & 0xFF;
    dbbus_tx_data[4] = ( Type1[128+0x0B] >> 8 ) & 0xFF;
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, 5);
    
    dbbus_tx_data[2] = 0x12 * 2;
    dbbus_tx_data[3] = Type1[128+0x12] & 0xFF;
    dbbus_tx_data[4] = ( Type1[128+0x12] >> 8 ) & 0xFF;
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, 5);
    
    dbbus_tx_data[2] = 0x15 * 2;
    dbbus_tx_data[3] = Type1[128+0x15] & 0xFF;
    dbbus_tx_data[4] = ( Type1[128+0x15] >> 8 ) & 0xFF;
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, 5);        
}

static void ito_test_set_v( uint8_t Enable, uint8_t Prs)	
{
    uint16_t    u16RegValue;        
    
    
    u16RegValue = _msg_ReadReg( 0x12, 0x08);   
    u16RegValue = u16RegValue & 0xF1; 							
    if ( Prs == 0 )
    {
    	_msg_WriteReg( 0x12, 0x08, u16RegValue| 0x0C); 		
    }
    else if ( Prs == 1 )
    {
    	_msg_WriteReg( 0x12, 0x08, u16RegValue| 0x0E); 		     	
    }
    else
    {
    	_msg_WriteReg( 0x12, 0x08, u16RegValue| 0x02); 			
    }    
    
    if ( Enable )
    {
        u16RegValue = _msg_ReadReg( 0x11, 0x06);    
        _msg_WriteReg( 0x11, 0x06, u16RegValue| 0x03);   	
    }
    else
    {
        u16RegValue = _msg_ReadReg( 0x11, 0x06);    
        u16RegValue = u16RegValue & 0xFC;					
        _msg_WriteReg( 0x11, 0x06, u16RegValue);         
    }

}

static void ito_test_set_c( uint8_t Csub_Step )
{
    uint8_t i;
    uint8_t dbbus_tx_data[MAX_CHNL_NUM+3];
    uint8_t HighLevel_Csub = false;
    uint8_t Csub_new;
     
    dbbus_tx_data[0] = 0x10;
    dbbus_tx_data[1] = 0x11;        
    dbbus_tx_data[2] = 0x84;        
    for( i = 0; i < MAX_CHNL_NUM; i++ )
    {
		Csub_new = Csub_Step;        
        HighLevel_Csub = false;   
        if( Csub_new > 0x1F )
        {
            Csub_new = Csub_new - 0x14;
            HighLevel_Csub = true;
        }
           
        dbbus_tx_data[3+i] =    Csub_new & 0x1F;        
        if( HighLevel_Csub == true )
        {
            dbbus_tx_data[3+i] |= BIT5;
        }
    }
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, MAX_CHNL_NUM+3);

    dbbus_tx_data[2] = 0xB4;        
    _msg_WriteI2CSeq(FW_ADDR_MSG, dbbus_tx_data, MAX_CHNL_NUM+3);
}

static void ito_test_sw( void )
{
    _msg_WriteReg( 0x11, 0x00, 0xFFFF );
    _msg_WriteReg( 0x11, 0x00, 0x0000 );
    mdelay( 50 );
}



static void ito_test_first( uint8_t item_id , int16_t* s16_raw_data)		
{
	uint8_t     result = 0,loop;
	uint8_t     dbbus_tx_data[9];
	uint8_t     i,j;
    int16_t     s16_raw_data_tmp[48]={0};
	uint8_t     num_of_sensor, num_of_sensor2,total_sensor;
	uint16_t	u16RegValue;
    uint8_t 	*pMapping=NULL;
    
    
	num_of_sensor = 0;
	num_of_sensor2 = 0;	
	
    ITO_TEST_DEBUG("ito_test_first item_id=%d\n",item_id);
	_msg_WriteReg( 0x0F, 0xE6, 0x01 );

	_msg_WriteReg( 0x1E, 0x24, 0x0500 );
	_msg_WriteReg( 0x1E, 0x2A, 0x0000 );
	_msg_WriteReg( 0x1E, 0xE6, 0x6E00 );
	_msg_WriteReg( 0x1E, 0xE8, 0x0071 );
	    
    if ( item_id == 40 )    			
    {
        pMapping = &MAP1[0];
        if ( ito_test_2r )
		{
			total_sensor = ito_test_trianglenum/2; 
		}
		else
		{
		    total_sensor = ito_test_trianglenum/2 + ito_test_keynum + ito_test_dummynum;
		}
    }
    else if( item_id == 41 )    		
    {
        pMapping = &MAP2[0];
        if ( ito_test_2r )
		{
			total_sensor = ito_test_trianglenum/2; 
		}
		else
		{
		    total_sensor = ito_test_trianglenum/2 + ito_test_keynum + ito_test_dummynum;
		}
    }
    else if( item_id == 42 )    		
    {
        pMapping = &MAP3[0];      
        total_sensor =  ito_test_trianglenum + ito_test_keynum+ ito_test_dummynum; 
    }
        	    
	    
	loop = 1;
	if ( item_id != 42 )
	{
	    if(total_sensor>11)
        {
            loop = 2;
        }
	}	
    ITO_TEST_DEBUG("loop=%d\n",loop);
	for ( i = 0; i < loop; i++ )
	{
		if ( i == 0 )
		{
			ito_test_send_data_in( item_id - 36 );
		}
		else
		{ 
			if ( item_id == 40 ) 
				ito_test_send_data_in( 9 );
			else 		
				ito_test_send_data_in( 10 );
		}
	
		ito_test_set_v(1,0);    
		u16RegValue = _msg_ReadReg( 0x11, 0x0E);    			
		_msg_WriteReg( 0x11, 0x0E, u16RegValue | BIT11 );				 		
	
		if ( g_LTP == 1 )
	    	ito_test_set_c( 32 );	    	
		else	    	
	    	ito_test_set_c( 0 );
	    
		ito_test_sw();
		
		if ( i == 0 )	 
        {      
            num_of_sensor=ito_test_get_data_out(  s16_raw_data_tmp );
            ITO_TEST_DEBUG("num_of_sensor=%d;\n",num_of_sensor);
        }
		else	
        {      
            num_of_sensor2=ito_test_get_data_out(  &s16_raw_data_tmp[num_of_sensor] );
            ITO_TEST_DEBUG("num_of_sensor=%d;num_of_sensor2=%d\n",num_of_sensor,num_of_sensor2);
        }
	}
    for ( j = 0; j < total_sensor ; j ++ )
	{
		if ( g_LTP == 1 )
			s16_raw_data[pMapping[j]] = s16_raw_data_tmp[j] + 4096;
		else
			s16_raw_data[pMapping[j]] = s16_raw_data_tmp[j];	
	}	

	return;
}
typedef enum
{
	ITO_TEST_OK = 0,
	ITO_TEST_FAIL,
	ITO_TEST_GET_TP_TYPE_ERROR,
} ITO_TEST_RET;
ITO_TEST_RET ito_test_second (u8 item_id)
{
	u8 i = 0;
    
	s32  s16_raw_data_jg_tmp1 = 0;
	s32  s16_raw_data_jg_tmp2 = 0;
	s32  jg_tmp1_avg_Th_max =0;
	s32  jg_tmp1_avg_Th_min =0;
	s32  jg_tmp2_avg_Th_max =0;
	s32  jg_tmp2_avg_Th_min =0;

	u8  Th_Tri = 25;        
	u8  Th_bor = 25;        

	if ( item_id == 40 )    			
    {
        for (i=0; i<(ito_test_trianglenum/2)-2; i++)
        {
			s16_raw_data_jg_tmp1 += s16_raw_data_1[MAP40_1[i]];
		}
		for (i=0; i<2; i++)
        {
			s16_raw_data_jg_tmp2 += s16_raw_data_1[MAP40_2[i]];
		}
    }
    else if( item_id == 41 )    		
    {
        for (i=0; i<(ito_test_trianglenum/2)-2; i++)
        {
			s16_raw_data_jg_tmp1 += s16_raw_data_2[MAP41_1[i]];
		}
		for (i=0; i<2; i++)
        {
			s16_raw_data_jg_tmp2 += s16_raw_data_2[MAP41_2[i]];
		}
    }

	    jg_tmp1_avg_Th_max = (s16_raw_data_jg_tmp1 / ((ito_test_trianglenum/2)-2)) * ( 100 + Th_Tri) / 100 ;
	    jg_tmp1_avg_Th_min = (s16_raw_data_jg_tmp1 / ((ito_test_trianglenum/2)-2)) * ( 100 - Th_Tri) / 100 ;
        jg_tmp2_avg_Th_max = (s16_raw_data_jg_tmp2 / 2) * ( 100 + Th_bor) / 100 ;
	    jg_tmp2_avg_Th_min = (s16_raw_data_jg_tmp2 / 2 ) * ( 100 - Th_bor) / 100 ;
	
        ITO_TEST_DEBUG("item_id=%d;sum1=%d;max1=%d;min1=%d;sum2=%d;max2=%d;min2=%d\n",item_id,s16_raw_data_jg_tmp1,jg_tmp1_avg_Th_max,jg_tmp1_avg_Th_min,s16_raw_data_jg_tmp2,jg_tmp2_avg_Th_max,jg_tmp2_avg_Th_min);

	if ( item_id == 40 ) 
	{
		for (i=0; i<(ito_test_trianglenum/2)-2; i++)
	    {
			if (s16_raw_data_1[MAP40_1[i]] > jg_tmp1_avg_Th_max || s16_raw_data_1[MAP40_1[i]] < jg_tmp1_avg_Th_min) 
				return ITO_TEST_FAIL;
		}
		
		for (i=0; i<2; i++)
	    {
			if (s16_raw_data_1[MAP40_2[i]] > jg_tmp2_avg_Th_max || s16_raw_data_1[MAP40_2[i]] < jg_tmp2_avg_Th_min) 
				return ITO_TEST_FAIL;
		} 
	}

	if ( item_id == 41 ) 
	{
		for (i=0; i<(ito_test_trianglenum/2)-2; i++)
	    {
			if (s16_raw_data_2[MAP41_1[i]] > jg_tmp1_avg_Th_max || s16_raw_data_2[MAP41_1[i]] < jg_tmp1_avg_Th_min) 
				return ITO_TEST_FAIL;
		}
		
		for (i=0; i<2; i++)
	    {
			if (s16_raw_data_2[MAP41_2[i]] > jg_tmp2_avg_Th_max || s16_raw_data_2[MAP41_2[i]] < jg_tmp2_avg_Th_min) 
				return ITO_TEST_FAIL;
		} 
	}

	return ITO_TEST_OK;
	
}
ITO_TEST_RET ito_test_second_2r (u8 item_id)
{
	u8 i = 0;
    
	s32  s16_raw_data_jg_tmp1 = 0;
	s32  s16_raw_data_jg_tmp2 = 0;
	s32  s16_raw_data_jg_tmp3 = 0;
	s32  s16_raw_data_jg_tmp4 = 0;
	
	s32  jg_tmp1_avg_Th_max =0;
	s32  jg_tmp1_avg_Th_min =0;
	s32  jg_tmp2_avg_Th_max =0;
	s32  jg_tmp2_avg_Th_min =0;
	s32  jg_tmp3_avg_Th_max =0;
	s32  jg_tmp3_avg_Th_min =0;
	s32  jg_tmp4_avg_Th_max =0;
	s32  jg_tmp4_avg_Th_min =0;


	
	s32  Th_fst = 1000;  // new threshold, make sure all data not less than 1000.
	u8  Th_Tri = 25;    // non-border threshold    
	u8  Th_bor = 25;    // border threshold    

	if ( item_id == 40 )    			
    {
        for (i=0; i<(ito_test_trianglenum/4)-2; i++)
        {
			s16_raw_data_jg_tmp1 += s16_raw_data_1[MAP40_1[i]];  //first region: non-border 
		}
		for (i=0; i<2; i++)
        {
			s16_raw_data_jg_tmp2 += s16_raw_data_1[MAP40_2[i]];  //first region: border
		}

		for (i=0; i<(ito_test_trianglenum/4)-2; i++)
        {
			s16_raw_data_jg_tmp3 += s16_raw_data_1[MAP40_3[i]];  //second region: non-border
		}
		for (i=0; i<2; i++)
        {
			s16_raw_data_jg_tmp4 += s16_raw_data_1[MAP40_4[i]];  //second region: border
		}
    }



	
    else if( item_id == 41 )    		
    {
        for (i=0; i<(ito_test_trianglenum/4)-2; i++)
        {
			s16_raw_data_jg_tmp1 += s16_raw_data_2[MAP41_1[i]];  //first region: non-border
		}
		for (i=0; i<2; i++)
        {
			s16_raw_data_jg_tmp2 += s16_raw_data_2[MAP41_2[i]];  //first region: border
		}
		for (i=0; i<(ito_test_trianglenum/4)-2; i++)
        {
			s16_raw_data_jg_tmp3 += s16_raw_data_2[MAP41_3[i]];  //second region: non-border
		}
		for (i=0; i<2; i++)
        {
			s16_raw_data_jg_tmp4 += s16_raw_data_2[MAP41_4[i]];  //second region: border
		}
    }

	    jg_tmp1_avg_Th_max = (s16_raw_data_jg_tmp1 / ((ito_test_trianglenum/4)-2)) * ( 100 + Th_Tri) / 100 ;
	    jg_tmp1_avg_Th_min = (s16_raw_data_jg_tmp1 / ((ito_test_trianglenum/4)-2)) * ( 100 - Th_Tri) / 100 ;
        jg_tmp2_avg_Th_max = (s16_raw_data_jg_tmp2 / 2) * ( 100 + Th_bor) / 100 ;
	    jg_tmp2_avg_Th_min = (s16_raw_data_jg_tmp2 / 2) * ( 100 - Th_bor) / 100 ;
		jg_tmp3_avg_Th_max = (s16_raw_data_jg_tmp3 / ((ito_test_trianglenum/4)-2)) * ( 100 + Th_Tri) / 100 ;
	    jg_tmp3_avg_Th_min = (s16_raw_data_jg_tmp3 / ((ito_test_trianglenum/4)-2)) * ( 100 - Th_Tri) / 100 ;
        jg_tmp4_avg_Th_max = (s16_raw_data_jg_tmp4 / 2) * ( 100 + Th_bor) / 100 ;
	    jg_tmp4_avg_Th_min = (s16_raw_data_jg_tmp4 / 2) * ( 100 - Th_bor) / 100 ;
		
	
        ITO_TEST_DEBUG("item_id=%d;sum1=%d;max1=%d;min1=%d;sum2=%d;max2=%d;min2=%d;sum3=%d;max3=%d;min3=%d;sum4=%d;max4=%d;min4=%d;\n",item_id,s16_raw_data_jg_tmp1,jg_tmp1_avg_Th_max,jg_tmp1_avg_Th_min,s16_raw_data_jg_tmp2,jg_tmp2_avg_Th_max,jg_tmp2_avg_Th_min,s16_raw_data_jg_tmp3,jg_tmp3_avg_Th_max,jg_tmp3_avg_Th_min,s16_raw_data_jg_tmp4,jg_tmp4_avg_Th_max,jg_tmp4_avg_Th_min);




	if ( item_id == 40 ) 
	{
		for (i=0; i<(ito_test_trianglenum/4)-2; i++)
	    {
			if (s16_raw_data_1[MAP40_1[i]] > jg_tmp1_avg_Th_max || s16_raw_data_1[MAP40_1[i]] < jg_tmp1_avg_Th_min || s16_raw_data_1[MAP40_1[i]] < Th_fst) 
				return ITO_TEST_FAIL;
		}
        
		for (i=0; i<2; i++)
	    {
			if (s16_raw_data_1[MAP40_2[i]] > jg_tmp2_avg_Th_max || s16_raw_data_1[MAP40_2[i]] < jg_tmp2_avg_Th_min || s16_raw_data_1[MAP40_2[i]] < Th_fst) 
				return ITO_TEST_FAIL;
		} 
		
		for (i=0; i<(ito_test_trianglenum/4)-2; i++)
	    {
			if (s16_raw_data_1[MAP40_3[i]] > jg_tmp3_avg_Th_max || s16_raw_data_1[MAP40_3[i]] < jg_tmp3_avg_Th_min || s16_raw_data_1[MAP40_3[i]] < Th_fst) 
				return ITO_TEST_FAIL;
		}
		
		for (i=0; i<2; i++)
	    {
			if (s16_raw_data_1[MAP40_4[i]] > jg_tmp4_avg_Th_max || s16_raw_data_1[MAP40_4[i]] < jg_tmp4_avg_Th_min || s16_raw_data_1[MAP40_4[i]] < Th_fst) 
				return ITO_TEST_FAIL;
		} 
	}

	if ( item_id == 41 ) 
	{
		for (i=0; i<(ito_test_trianglenum/4)-2; i++)
	    {
			if (s16_raw_data_2[MAP41_1[i]] > jg_tmp1_avg_Th_max || s16_raw_data_2[MAP41_1[i]] < jg_tmp1_avg_Th_min || s16_raw_data_2[MAP41_1[i]] < Th_fst) 
				return ITO_TEST_FAIL;
		}
		
		for (i=0; i<2; i++)
	    {
			if (s16_raw_data_2[MAP41_2[i]] > jg_tmp2_avg_Th_max || s16_raw_data_2[MAP41_2[i]] < jg_tmp2_avg_Th_min || s16_raw_data_2[MAP41_2[i]] < Th_fst) 
				return ITO_TEST_FAIL;
		}
		
		for (i=0; i<(ito_test_trianglenum/4)-2; i++)
	    {
			if (s16_raw_data_2[MAP41_3[i]] > jg_tmp3_avg_Th_max || s16_raw_data_2[MAP41_3[i]] < jg_tmp3_avg_Th_min || s16_raw_data_2[MAP41_3[i]] < Th_fst) 
				return ITO_TEST_FAIL;
		}
		
		for (i=0; i<2; i++)
	    {
			if (s16_raw_data_2[MAP41_4[i]] > jg_tmp4_avg_Th_max || s16_raw_data_2[MAP41_4[i]] < jg_tmp4_avg_Th_min || s16_raw_data_2[MAP41_4[i]] < Th_fst) 
				return ITO_TEST_FAIL;
		} 

		
	}

	return ITO_TEST_OK;
	
}
static ITO_TEST_RET ito_test_interface(void)
{
    ITO_TEST_RET ret = ITO_TEST_OK;
    uint16_t i = 0;
    bItoTesting = 1;
    ito_test_set_iic_rate(50000);
    ITO_TEST_DEBUG("start\n");
    _msg_disable_irq();
	_msg_resetHW();
    if(!ito_test_choose_TpType())
    {
        ITO_TEST_DEBUG("choose tpType fail\n");
        ret = ITO_TEST_GET_TP_TYPE_ERROR;
        goto ITO_TEST_END;
    }
    _msg_EnterSerialDebugMode();
    mdelay(100);
    ITO_TEST_DEBUG("EnterSerialDebugMode\n");
    _msg_WriteReg8Bit ( 0x0F, 0xE6, 0x01 );
    _msg_WriteReg ( 0x3C, 0x60, 0xAA55 );
    ITO_TEST_DEBUG("stop mcu and disable watchdog V.005\n");   
    mdelay(50);
    
	for(i = 0;i < 48;i++)
	{
		s16_raw_data_1[i] = 0;
		s16_raw_data_2[i] = 0;
		s16_raw_data_3[i] = 0;
	}	
	
    ito_test_first(40, s16_raw_data_1);
    ITO_TEST_DEBUG("40 get s16_raw_data_1\n");
    if(ito_test_2r)
    {
        ret=ito_test_second_2r(40);
    }
    else
    {
        ret=ito_test_second(40);
    }
    if(ITO_TEST_FAIL==ret)
    {
        goto ITO_TEST_END;
    }
    
    ito_test_first(41, s16_raw_data_2);
    ITO_TEST_DEBUG("41 get s16_raw_data_2\n");
    if(ito_test_2r)
    {
        ret=ito_test_second_2r(41);
    }
    else
    {
        ret=ito_test_second(41);
    }
    if(ITO_TEST_FAIL==ret)
    {
        goto ITO_TEST_END;
    }
    
    ito_test_first(42, s16_raw_data_3);
    ITO_TEST_DEBUG("42 get s16_raw_data_3\n");
    
    ITO_TEST_END:
    ito_test_set_iic_rate(100000);
	_msg_resetHW();
    _msg_enable_irq();
    bItoTesting = 0;
    ITO_TEST_DEBUG("end\n");
    return ret;
}
#define ITO_TEST_AUTHORITY 0777 
static struct proc_dir_entry *msg_ito_test = NULL;
static struct proc_dir_entry *debug = NULL;
static struct proc_dir_entry *debug_on_off = NULL;
#define PROC_MSG_ITO_TESE      "msg-ito-test"
#define PROC_ITO_TEST_DEBUG      "debug"
#define PROC_ITO_TEST_DEBUG_ON_OFF     "debug-on-off"
ITO_TEST_RET g_ito_test_ret = ITO_TEST_OK;
static int ito_test_proc_read_debug(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int cnt= 0;
    
    g_ito_test_ret = ito_test_interface();
    if(ITO_TEST_OK==g_ito_test_ret)
    {
        ITO_TEST_DEBUG_MUST("ITO_TEST_OK");
    }
    else if(ITO_TEST_FAIL==g_ito_test_ret)
    {
        ITO_TEST_DEBUG_MUST("ITO_TEST_FAIL");
    }
    else if(ITO_TEST_GET_TP_TYPE_ERROR==g_ito_test_ret)
    {
        ITO_TEST_DEBUG_MUST("ITO_TEST_GET_TP_TYPE_ERROR");
    }
    
    *eof = 1;
    return cnt;
}

static int ito_test_proc_write_debug(struct file *file, const char *buffer, unsigned long count, void *data)
{    
    u16 i = 0;
    mdelay(5);
    ITO_TEST_DEBUG_MUST("ito_test_ret = %d",g_ito_test_ret);
    mdelay(5);
    for(i=0;i<48;i++)
    {
        ITO_TEST_DEBUG_MUST("data_1[%d]=%d;\n",i,s16_raw_data_1[i]);
    }
    mdelay(5);
    for(i=0;i<48;i++)
    {
        ITO_TEST_DEBUG_MUST("data_2[%d]=%d;\n",i,s16_raw_data_2[i]);
    }
    mdelay(5);
    for(i=0;i<48;i++)
    {
        ITO_TEST_DEBUG_MUST("data_3[%d]=%d;\n",i,s16_raw_data_3[i]);
    }
    mdelay(5);
    return count;
}
static int ito_test_proc_read_debug_on_off(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    int cnt= 0;
    
    bItoTestDebug = 1;
    ITO_TEST_DEBUG_MUST("on debug bItoTestDebug = %d",bItoTestDebug);
    
    *eof = 1;
    return cnt;
}

static int ito_test_proc_write_debug_on_off(struct file *file, const char *buffer, unsigned long count, void *data)
{    
    bItoTestDebug = 0;
    ITO_TEST_DEBUG_MUST("off debug bItoTestDebug = %d",bItoTestDebug);
    return count;
}
static void ito_test_create_entry(void)
{
    msg_ito_test = proc_mkdir(PROC_MSG_ITO_TESE, NULL);
    debug = create_proc_entry(PROC_ITO_TEST_DEBUG, ITO_TEST_AUTHORITY, msg_ito_test);
    debug_on_off= create_proc_entry(PROC_ITO_TEST_DEBUG_ON_OFF, ITO_TEST_AUTHORITY, msg_ito_test);

    if (NULL==debug) 
    {
        ITO_TEST_DEBUG_MUST("create_proc_entry ITO TEST DEBUG failed\n");
    } 
    else 
    {
        debug->read_proc = ito_test_proc_read_debug;
        debug->write_proc = ito_test_proc_write_debug;
        ITO_TEST_DEBUG_MUST("create_proc_entry ITO TEST DEBUG OK\n");
    }
    if (NULL==debug_on_off) 
    {
        ITO_TEST_DEBUG_MUST("create_proc_entry ITO TEST ON OFF failed\n");
    } 
    else 
    {
        debug_on_off->read_proc = ito_test_proc_read_debug_on_off;
        debug_on_off->write_proc = ito_test_proc_write_debug_on_off;
        ITO_TEST_DEBUG_MUST("create_proc_entry ITO TEST ON OFF OK\n");
    }
}
#endif

MODULE_DESCRIPTION("msg Touchscreen Driver");
MODULE_LICENSE("GPL");
