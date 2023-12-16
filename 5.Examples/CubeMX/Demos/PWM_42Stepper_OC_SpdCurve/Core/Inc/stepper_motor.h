
#ifndef __STEPPER_MOTOR_H
#define __STEPPER_MOTOR_H

#include "stm32h7xx_hal.h"
#include "main.h"
#include "malloc.h"
#include "stdbool.h"
#include "stdio.h"

#define STEPPER_ENABLE()    HAL_GPIO_WritePin(MOTOR_EN_GPIO_Port, MOTOR_EN_Pin, GPIO_PIN_SET)
#define STEPPER_DISABLE()   HAL_GPIO_WritePin(MOTOR_EN_GPIO_Port, MOTOR_EN_Pin, GPIO_PIN_RESET)

#define STEPPER_DIR_CW()    HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin, GPIO_PIN_SET)
#define STEPPER_DIR_CCW()   HAL_GPIO_WritePin(MOTOR_DIR_GPIO_Port, MOTOR_DIR_Pin, GPIO_PIN_RESET)

#define STEPPER_PWM_START() HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_1)
#define STEPPER_PWM_STOP()  HAL_TIM_PWM_Stop_IT(&htim1, TIM_CHANNEL_1)

enum STA {
    STOP = 0, /* �Ӽ�������״̬��ֹͣ*/
    ACCEL,    /* �Ӽ�������״̬�����ٽ׶�*/
    DECEL,    /* �Ӽ�������״̬�����ٽ׶�*/
    RUN       /* �Ӽ�������״̬�����ٽ׶�*/
};

enum DIR {
    CCW = 0, /* ��ʱ�� */
    CW       /* ˳ʱ�� */
};

enum EN {
    EN_ON = 0, /* ʧ���ѻ����� */
    EN_OFF     /* ʹ���ѻ����� ʹ�ܺ���ֹͣ��ת */
};

#define STEPPER_MOTOR_1 1

#define ST_DIR(x)       x ? STEPPER_DIR_CW() : STEPPER_DIR_CCW()
#define ST_EN(x)        x ? STEPPER_ENABLE() : STEPPER_DISABLE()

/* �ⲿ�ӿں���*/
void stepper_init(uint16_t arr, uint16_t psc); /* ��������ӿڳ�ʼ�� */
void stepper_star(uint8_t motor_num);          /* ����������� */
void stepper_stop(uint8_t motor_num);          /* �رղ������ */

#define DEMO 3

#if DEMO == 1

typedef struct
{
    __IO uint8_t  run_state;   /* �����ת״̬ */
    __IO uint8_t  dir;         /* �����ת���� */
    __IO int32_t  step_delay;  /* �¸��������ڣ�ʱ������������ʱΪ���ٶ� */
    __IO uint32_t decel_start; /* ��ʼ����λ�� */
    __IO int32_t  decel_val;   /* ���ٽ׶β��� */
    __IO int32_t  min_delay;   /* �ٶ���죬����ֵ��С��ֵ(����ٶȣ������ٶ��ٶ�) */
    __IO int32_t  accel_count; /* �Ӽ��ٽ׶μ���ֵ */
} speedRampData;

#define MAX_STEP_ANGLE 0.225               /* ��С����(1.8/MICRO_STEP) */
#define PAI            3.1415926           /* Բ����*/
#define FSPR           200                 /* ���������Ȧ���� */
#define MICRO_STEP     8                   /* �������������ϸ���� */
#define T1_FREQ        1e6                 /* Ƶ��ftֵ */
#define SPR            (FSPR * MICRO_STEP) /* ��תһȦ��Ҫ�������� */

/* ��ѧ���� */

#define ALPHA          ((float)(2 * PAI / SPR)) /* �� = 2*pi/spr */
#define A_T_x10        ((float)(10 * ALPHA * T1_FREQ))
#define T1_FREQ_148    ((float)((T1_FREQ * 0.69) / 10)) /* 0.69Ϊ�������ֵ */
#define A_SQ           ((float)(2 * 100000 * ALPHA))
#define A_x200         ((float)(200 * ALPHA)) /* 2*10*10*a/10 */

void create_t_ctrl_param(int32_t step, uint32_t accel, uint32_t decel, uint32_t speed); /* ���μӼ��ٿ��ƺ��� */

//--------------

#elif DEMO == 2

#define T1_FREQ                1e6  // Hz
#define FSPR                   200  /* ���������Ȧ���� */
#define MICRO_STEP             8
#define SPR                    (FSPR * MICRO_STEP) /* ��Ȧ����Ҫ�������� */

#define ROUNDPS_2_STEPPS(rpm)  ((rpm) * SPR / 60)                 /* ���ݵ��ת�٣�r/min�������������٣�step/s�� */
#define MIDDLEVELOCITY(vo, vt) (((vo) + (vt)) / 2)                /* S�ͼӼ��ټ��ٶε��е��ٶ�  */
#define INCACCEL(vo, v, t)     ((2 * ((v) - (vo))) / pow((t), 2)) /* �Ӽ��ٶ�:���ٶ�������   V - V0 = 1/2 * J * t^2 */
#define INCACCELSTEP(j, t)     (((j) * pow((t), 3)) / 6.0f)       /* �Ӽ��ٶε�λ����(����)  S = 1/6 * J * t^3 */
#define ACCEL_TIME(t)          ((t) / 2)                          /* �Ӽ��ٶκͼ����ٶε�ʱ������ȵ� */
#define SPEED_MIN              (T1_FREQ / (65535.0f))             /* ���Ƶ��/�ٶ� */

typedef struct {
    int32_t vo;         /*  ���ٶ� ��λ step/s */
    int32_t vt;         /*  ĩ�ٶ� ��λ step/s */
    int32_t accel_step; /*  ���ٶεĲ�����λ step */
    int32_t decel_step; /*  ���ٶεĲ�����λ step */
    float*  accel_tab;  /*  �ٶȱ�� ��λ step/s �������������Ƶ�� */
    float*  decel_tab;  /*  �ٶȱ�� ��λ step/s �������������Ƶ�� */
    float*  ptr;        /*  �ٶ�ָ�� */
    int32_t dec_point;  /*  ���ٵ� */
    int32_t step;
    int32_t step_pos;
} speed_calc_t;

typedef enum {
    STATE_ACCEL    = 1, /* �������״̬ */
    STATE_AVESPEED = 2, /* �������״̬ */
    STATE_DECEL    = 3, /* �������״̬ */
    STATE_STOP     = 0, /* ���ֹͣ״̬ */
    STATE_IDLE     = 4, /* �������״̬ */
} motor_state_typedef;

void    stepmotor_move_rel(int32_t vo, int32_t vt, float AcTime, float DeTime, int32_t step); /* S�ͼӼ����˶����ƺ��� */
uint8_t calc_speed(int32_t vo, int32_t vt, float time);                                       /* �����ٶȱ� */

#elif DEMO == 3  // δ��

/* ֱ�߲岹���� */

#define AXIS_X                    0 /* X���� */
#define AXIS_Y                    1 /* Y���� */
#define LINE                      0

/* X����������Ŷ��� */
#define STEPMOTOR_TIM_CHANNEL1    TIM_CHANNEL_3 /* ��ʱ��8ͨ��3 X�� */
#define STEPMOTOR_TIM_PULSE_PIN_X GPIO_PIN_7    /* ��������X�Ჽ�������� */

#define STEPMOTOR_X_DIR_PORT      GPIOB      /* X�᷽��� */
#define STEPMOTOR_X_DIR_PIN       GPIO_PIN_2 /* X�᷽��� */

#define STEPMOTOR_X_ENA_PORT      GPIOF         /* X��ʹ�ܽ� */
#define STEPMOTOR_X_ENA_PIN       GPIO_PIN_11   /* X��ʹ�ܽ� */

/* Y����������Ŷ��� */
#define STEPMOTOR_TIM_CHANNEL2    TIM_CHANNEL_4 /* ��ʱ��8ͨ��4 Y�� */
#define STEPMOTOR_TIM_PULSE_PIN_Y GPIO_PIN_9    /* ��������Y�Ჽ�������� */

#define STEPMOTOR_Y_DIR_PORT      GPIOH      /* Y�᷽��� */
#define STEPMOTOR_Y_DIR_PIN       GPIO_PIN_2 /* Y�᷽��� */

#define STEPMOTOR_Y_ENA_PORT      GPIOH      /* Y��ʹ�ܽ� */
#define STEPMOTOR_Y_ENA_PIN       GPIO_PIN_3 /* Y��ʹ�ܽ� */

/* ------------------------���õ������,����axis:��ǰ���---------------------------- */
#define ST_LINE_DIR(x, axis)                                                                                                                                                       \
    do {                                                                                                                                                                           \
        x ? HAL_GPIO_WritePin(st_motor[axis].dir_port, st_motor[axis].dir_pin, GPIO_PIN_SET) : HAL_GPIO_WritePin(st_motor[axis].dir_port, st_motor[axis].dir_pin, GPIO_PIN_RESET); \
    } while (0)
/* -----------------------���õ��ʹ��,����axis:��ǰ��� -----------------------------*/
#define ST_LINE_EN(x, axis)                                                                                                                                                    \
    do {                                                                                                                                                                       \
        x ? HAL_GPIO_WritePin(st_motor[axis].en_port, st_motor[axis].en_pin, GPIO_PIN_SET) : HAL_GPIO_WritePin(st_motor[axis].en_port, st_motor[axis].en_pin, GPIO_PIN_RESET); \
    } while (0)

typedef enum /* ���״̬ */
{
    STATE_STOP = 0,
    STATE_RUN  = 1,
} st_motor_status_def;

typedef struct {
    uint16_t      pulse_pin;     /* ��ʱ������������� */
    uint32_t      pulse_channel; /* ��ʱ���������ͨ�� */
    uint16_t      en_pin;        /* ���ʹ�����ű�� */
    uint16_t      dir_pin;       /* ����������ű�� */
    GPIO_TypeDef* dir_port;      /* ����������Ŷ˿� */
    GPIO_TypeDef* en_port;       /* ���ʹ�����Ŷ˿� */
} st_motor_ctr_def;

/*  �岹�㷨���Ͷ��� */
typedef struct {
    __IO uint8_t  moving_mode; /* �˶�ģʽ */
    __IO uint8_t  inter_dir;   /* �岹���� */
    __IO uint8_t  qua_points;  /* ���޵� */
    __IO uint8_t  x_dir;       /* X�᷽�� */
    __IO uint8_t  y_dir;       /* Y�᷽�� */
    __IO int32_t  end_x;       /* �յ�����X */
    __IO int32_t  end_y;       /* �յ�����Y */
    __IO uint32_t end_pulse;   /* �յ�λ���ܵ������� */
    __IO uint32_t act_axis;    /* ��� */
    __IO int32_t  f_e;         /* �������� */
} inter_pol_def;

void stepper_pwmt_speed(uint16_t speed, uint32_t Channel);             /* �����ٶ� */
void line_inpolation(int32_t coordsX, int32_t coordsY, int32_t Speed); /* ʵ����������ֱ�߲岹 */

#endif

#endif
