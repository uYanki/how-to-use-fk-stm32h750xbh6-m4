

#include "stepper_motor.h"

#include "math.h"

extern TIM_HandleTypeDef htim1;

void stepper_star(uint8_t motor_num)
{
    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_1);
}

void stepper_stop(uint8_t motor_num)
{
    HAL_TIM_OC_Stop_IT(&htim1, TIM_CHANNEL_1);
}

#if DEMO == 1

/********************************************���μӼ���***********************************************/
speedRampData g_srd             = {STOP, CW, 0, 0, 0, 0, 0}; /* �Ӽ��ٱ��� */
__IO int32_t  g_step_position   = 0;                         /* ��ǰλ�� */
__IO uint8_t  g_motion_sta      = 0;                         /* �Ƿ����˶���0��ֹͣ��1���˶� */
__IO uint32_t g_add_pulse_count = 0;                         /* ��������ۼ� */

/*
 * @brief       ���������˶����Ʋ���
 * @param       step���ƶ��Ĳ��� (����Ϊ˳ʱ�룬����Ϊ��ʱ��).
 * @param       accel  ���ٶ�,ʵ��ֵΪaccel*0.1*rad/sec^2  10������2��������һ������������
 * @param       decel  ���ٶ�,ʵ��ֵΪdecel*0.1*rad/sec^2
 * @param       speed  ����ٶ�,ʵ��ֵΪspeed*0.1*rad/sec
 * @retval      ��
 */
void create_t_ctrl_param(int32_t step, uint32_t accel, uint32_t decel, uint32_t speed)
{
    __IO uint16_t tim_count; /* �ﵽ����ٶ�ʱ�Ĳ���*/
    __IO uint32_t max_s_lim; /* ����Ҫ��ʼ���ٵĲ������������û�дﵽ����ٶȣ�*/
    __IO uint32_t accel_lim;
    if (g_motion_sta != STOP) /* ֻ�����������ֹͣ��ʱ��ż���*/
    {
        return;
    }
    if (step < 0) /* ����Ϊ���� */
    {
        g_srd.dir = CCW; /* ��ʱ�뷽����ת */
        ST_DIR(CCW);
        step = -step; /* ��ȡ��������ֵ */
    }
    else
    {
        g_srd.dir = CW; /* ˳ʱ�뷽����ת */
        ST_DIR(CW);
    }

    if (step == 1) /* ����Ϊ1 */
    {
        g_srd.accel_count = -1;    /* ֻ�ƶ�һ�� */
        g_srd.run_state   = DECEL; /* ����״̬. */
        g_srd.step_delay  = 1000;  /* Ĭ���ٶ� */
    }
    else if (step != 0) /* ���Ŀ���˶�������Ϊ0*/
    {
        /*��������ٶȼ���, ����õ�min_delay���ڶ�ʱ���ļ�������ֵ min_delay = (alpha / t)/ w*/
        g_srd.min_delay = (int32_t)(A_T_x10 / speed);  // ��������ʱ�ļ���ֵ

        /* ͨ�������һ��(c0) �Ĳ�����ʱ���趨���ٶȣ�����accel��λΪ0.1rad/sec^2
         step_delay = 1/tt * sqrt(2*alpha/accel)
         step_delay = ( tfreq*0.69/10 )*10 * sqrt( (2*alpha*100000) / (accel*10) )/100 */

        g_srd.step_delay = (int32_t)((T1_FREQ_148 * sqrt(A_SQ / accel)) / 10); /* c0 */

        max_s_lim = (uint32_t)(speed * speed / (A_x200 * accel / 10)); /* ������ٲ�֮��ﵽ����ٶȵ����� max_s_lim = speed^2 / (2*alpha*accel) */

        if (max_s_lim == 0) /* ����ﵽ����ٶ�С��0.5�������ǽ���������Ϊ0,��ʵ�����Ǳ����ƶ�����һ�����ܴﵽ��Ҫ���ٶ� */
        {
            max_s_lim = 1;
        }
        accel_lim = (uint32_t)(step * decel / (accel + decel)); /* ���ﲻ��������ٶ� ������ٲ�֮�����Ǳ��뿪ʼ���� n1 = (n1+n2)decel / (accel + decel) */

        if (accel_lim == 0) /* ����һ�� ��һ������*/
        {
            accel_lim = 1;
        }
        if (accel_lim <= max_s_lim) /* ���ٽ׶ε���������ٶȾ͵ü��١�����ʹ�������������ǿ��Լ�������ٽ׶β��� */
        {
            g_srd.decel_val = accel_lim - step; /* ���ٶεĲ��� */
        }
        else
        {
            g_srd.decel_val = -(max_s_lim * accel / decel); /* ���ٶεĲ��� */
        }
        if (g_srd.decel_val == 0) /* ����һ�� ��һ������ */
        {
            g_srd.decel_val = -1;
        }
        g_srd.decel_start = step + g_srd.decel_val; /* ���㿪ʼ����ʱ�Ĳ��� */

        if (g_srd.step_delay <= g_srd.min_delay) /* ���һ��ʼc0���ٶȱ����ٶ��ٶȻ��󣬾Ͳ���Ҫ���м����˶���ֱ�ӽ������� */
        {
            g_srd.step_delay = g_srd.min_delay;
            g_srd.run_state  = RUN;
        }
        else
        {
            g_srd.run_state = ACCEL;
        }
        g_srd.accel_count = 0; /* ��λ�Ӽ��ټ���ֵ */
    }
    g_motion_sta = 1; /* ���Ϊ�˶�״̬ */
    ST_EN(EN_ON);
    tim_count = __HAL_TIM_GET_COUNTER(&htim1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, tim_count + g_srd.step_delay / 2); /* ���ö�ʱ���Ƚ�ֵ */
    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_1);                                     /* ʹ�ܶ�ʱ��ͨ�� */
}

/**
 * @brief  ��ʱ���Ƚ��ж�
 * @param  htim����ʱ�����ָ��
 * @note   ��
 * @retval ��
 */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim)
{
    __IO uint32_t        tim_count        = 0;
    __IO uint32_t        tmp              = 0;
    uint16_t             new_step_delay   = 0; /* �����£��£�һ����ʱ���� */
    __IO static uint16_t last_accel_delay = 0; /* ���ٹ��������һ����ʱ���������ڣ� */
    __IO static uint32_t step_count       = 0; /* ���ƶ�����������*/
    __IO static int32_t  rest             = 0; /* ��¼new_step_delay�е������������һ������ľ��� */
    __IO static uint8_t  i                = 0; /* ��ʱ��ʹ�÷�תģʽ����Ҫ���������жϲ����һ���������� */

    tim_count = __HAL_TIM_GET_COUNTER(&htim1);
    tmp       = tim_count + g_srd.step_delay / 2; /* ����Cֵ�������Ҫ��ת���ε�������Ҫ����2 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, tmp);

    i++;        /* ��ʱ���жϴ�������ֵ */
    if (i == 2) /* 2�Σ�˵���Ѿ����һ���������� */
    {
        i = 0;                   /* ���㶨ʱ���жϴ�������ֵ */
        switch (g_srd.run_state) /* �Ӽ������߽׶� */
        {
            case STOP:
                step_count = 0; /* ���㲽�������� */
                rest       = 0; /* ������ֵ */
                /* �ر�ͨ��*/
                HAL_TIM_OC_Stop_IT(&htim1, TIM_CHANNEL_1);
                ST_EN(EN_OFF);
                g_motion_sta = 0; /* ���Ϊֹͣ״̬  */
                break;

            case ACCEL:
                g_add_pulse_count++; /* ֻ���ڼ�¼���λ��ת���˶��ٶ� */
                step_count++;        /* ������1*/
                if (g_srd.dir == CW)
                {
                    g_step_position++; /* ����λ�ü�1  ��¼����λ��ת�����ٶ�*/
                }
                else
                {
                    g_step_position--; /* ����λ�ü�1*/
                }
                g_srd.accel_count++;                                                                                 /* ���ټ���ֵ��1*/
                new_step_delay = g_srd.step_delay - (((2 * g_srd.step_delay) + rest) / (4 * g_srd.accel_count + 1)); /* ������(��)һ����������(ʱ����) */
                rest           = ((2 * g_srd.step_delay) + rest) % (4 * g_srd.accel_count + 1);                      /* �����������´μ��㲹��������������� */
                if (step_count >= g_srd.decel_start)                                                                 /* ����Ƿ�����Ҫ���ٵĲ��� */
                {
                    g_srd.accel_count = g_srd.decel_val; /* ���ټ���ֵΪ���ٽ׶μ���ֵ�ĳ�ʼֵ */
                    g_srd.run_state   = DECEL;           /* �¸����������ٽ׶� */
                }
                else if (new_step_delay <= g_srd.min_delay) /* ����Ƿ񵽴�����������ٶ� ����ֵԽС�ٶ�Խ�죬������ٶȺ�����ٶ���Ȼ����ͽ�������*/
                {
                    last_accel_delay = new_step_delay;  /* ������ٹ��������һ����ʱ���������ڣ�*/
                    new_step_delay   = g_srd.min_delay; /* ʹ��min_delay����Ӧ����ٶ�speed��*/
                    rest             = 0;               /* ������ֵ */
                    g_srd.run_state  = RUN;             /* ����Ϊ��������״̬ */
                }
                break;

            case RUN:
                g_add_pulse_count++;
                step_count++; /* ������1 */
                if (g_srd.dir == CW)
                {
                    g_step_position++; /* ����λ�ü�1 */
                }
                else
                {
                    g_step_position--; /* ����λ�ü�1*/
                }
                new_step_delay = g_srd.min_delay;    /* ʹ��min_delay����Ӧ����ٶ�speed��*/
                if (step_count >= g_srd.decel_start) /* ��Ҫ��ʼ���� */
                {
                    g_srd.accel_count = g_srd.decel_val;  /* ���ٲ�����Ϊ���ټ���ֵ */
                    new_step_delay    = last_accel_delay; /* �ӽ׶�������ʱ��Ϊ���ٽ׶ε���ʼ��ʱ(��������) */
                    g_srd.run_state   = DECEL;            /* ״̬�ı�Ϊ���� */
                }
                break;

            case DECEL:
                step_count++; /* ������1 */
                g_add_pulse_count++;
                if (g_srd.dir == CW)
                {
                    g_step_position++; /* ����λ�ü�1 */
                }
                else
                {
                    g_step_position--; /* ����λ�ü�1 */
                }
                g_srd.accel_count++;
                new_step_delay = g_srd.step_delay - (((2 * g_srd.step_delay) + rest) / (4 * g_srd.accel_count + 1)); /* ������(��)һ����������(ʱ����) */
                rest           = ((2 * g_srd.step_delay) + rest) % (4 * g_srd.accel_count + 1);                      /* �����������´μ��㲹��������������� */

                /* ����Ƿ�Ϊ���һ�� */
                if (g_srd.accel_count >= 0) /* �жϼ��ٲ����Ƿ�Ӹ�ֵ�ӵ�0�ǵĻ� ������� */
                {
                    g_srd.run_state = STOP;
                }
                break;
        }
        g_srd.step_delay = new_step_delay; /* Ϊ�¸�(�µ�)��ʱ(��������)��ֵ */
    }
}

#elif DEMO == 2

/****************************************S�ͼӼ����˶�*****************************************************/
volatile int32_t    g_step_pos     = 0;          /* ��ǰλ�� */
volatile uint16_t   g_toggle_pulse = 0;          /* ����Ƶ�ʿ��� */
motor_state_typedef g_motor_sta    = STATE_IDLE; /* ���״̬ */
speed_calc_t        g_calc_t       = {0};

__IO uint32_t g_add_pulse_count = 0; /* ��������ۼ� */

/**
 * @brief       �ٶȱ���㺯��
 * @param       vo,���ٶ�;vt,ĩ�ٶ�;time,����ʱ��
 * @retval      true���ɹ���false��ʧ��
 */
uint8_t calc_speed(int32_t vo, int32_t vt, float time)
{
    uint8_t is_dec       = false;
    int32_t i            = 0;
    int32_t vm           = 0;    /* �м���ٶ� */
    int32_t inc_acc_stp  = 0;    /* �Ӽ�������Ĳ��� */
    int32_t dec_acc_stp  = 0;    /* ����������Ĳ��� */
    int32_t accel_step   = 0;    /* ���ٻ������Ҫ�Ĳ��� */
    float   jerk         = 0;    /* �Ӽ��ٶ� */
    float   ti           = 0;    /* ʱ���� dt */
    float   sum_t        = 0;    /* ʱ���ۼ��� */
    float   delta_v      = 0;    /* �ٶȵ�����dv */
    float   ti_cube      = 0;    /* ʱ���������� */
    float*  velocity_tab = NULL; /* �ٶȱ��ָ�� */

    if (vo > vt)                            /* ���ٶȱ�ĩ�ٶȴ�,�������˶�,��ֵ�仯�������˶���ͬ */
    {                                       /* ֻ�ǽ����ʱ��ע�⽫�ٶȵ��� */
        is_dec      = true;                 /* ���ٶ� */
        g_calc_t.vo = ROUNDPS_2_STEPPS(vt); /* ת����λ ����:step/s */
        g_calc_t.vt = ROUNDPS_2_STEPPS(vo); /* ת����λ ĩ��:step/s */
    }
    else
    {
        is_dec      = false; /* ���ٶ� */
        g_calc_t.vo = ROUNDPS_2_STEPPS(vo);
        g_calc_t.vt = ROUNDPS_2_STEPPS(vt);
    }

    time = ACCEL_TIME(time);                /* �õ��Ӽ��ٶε�ʱ�� */
                                            // printf("time=%f\r\n",time);
    vm   = (g_calc_t.vo + g_calc_t.vt) / 2; /* �����е��ٶ� */

    jerk = fabs(2.0f * (vm - g_calc_t.vo) / (time * time)); /* �����е��ٶȼ���Ӽ��ٶ� */

    inc_acc_stp = (int32_t)(g_calc_t.vo * time + INCACCELSTEP(jerk, time)); /* �Ӽ�����Ҫ�Ĳ��� */

    dec_acc_stp = (int32_t)((g_calc_t.vt + g_calc_t.vo) * time - inc_acc_stp); /* ��������Ҫ�Ĳ��� S = vt * time - S1 */

    /* �����ڴ�ռ����ٶȱ� */
    accel_step = dec_acc_stp + inc_acc_stp; /* ������Ҫ�Ĳ��� */
    if (accel_step % 2 != 0)                /* ���ڸ���������ת�����������ݴ��������,���������1 */
    {
        accel_step += 1;
    }
    /* mallo�����ڴ�ռ�,�ǵ��ͷ� */
    velocity_tab = (float*)(malloc((accel_step + 1) * sizeof(float)));
    if (velocity_tab == NULL)
    {
        printf("�ڴ治��!���޸Ĳ���: %d\n", (accel_step + 1) * sizeof(float));
        return false;
    }
    /*
     * Ŀ���S���ٶ������Ƕ�ʱ��ķ���,�����ڿ��Ƶ����ʱ�������Բ����ķ�ʽ����,���������V-t������ת��
     * �õ�V-S����,����õ����ٶȱ��ǹ��ڲ������ٶ�ֵ.ʹ�ò������ÿһ�����ڿ��Ƶ���
     */
    /* �����һ���ٶ�,���ݵ�һ�����ٶ�ֵ�ﵽ��һ����ʱ�� */
    ti_cube         = 6.0f * 1.0f / jerk;       /* ����λ�ƺ�ʱ��Ĺ�ʽS = 1/6 * J * ti^3 ��1����ʱ��:ti^3 = 6 * 1 / jerk */
    ti              = pow(ti_cube, (1 / 3.0f)); /* ti */
    sum_t           = ti;
    delta_v         = 0.5f * jerk * pow(sum_t, 2); /* ��һ�����ٶ� */
    velocity_tab[0] = g_calc_t.vo + delta_v;

    /*****************************************************/
    if (velocity_tab[0] <= SPEED_MIN) /* �Ե�ǰ��ʱ��Ƶ�����ܴﵽ������ٶ� */
    {
        velocity_tab[0] = SPEED_MIN;
    }

    /*****************************************************/

    for (i = 1; i < accel_step; i++)
    {
        /* ����������ٶȾ��Ƕ�ʱ���������Ƶ��,���Լ����ÿһ����ʱ�� */
        /* �õ���i-1����ʱ�� */
        ti = 1.0f / velocity_tab[i - 1]; /* ���ÿ��һ����ʱ�� ti = 1 / Vn-1 */
        /* �Ӽ��ٶ��ٶȼ��� */
        if (i < inc_acc_stp)
        {
            sum_t += ti;                                   /* ��0��ʼ��i��ʱ���ۻ� */
            delta_v         = 0.5f * jerk * pow(sum_t, 2); /* �ٶȵı仯��: dV = 1/2 * jerk * ti^2 */
            velocity_tab[i] = g_calc_t.vo + delta_v;       /* �õ��Ӽ��ٶ�ÿһ����Ӧ���ٶ� */
            /* �����һ����ʱ��,ʱ�䲢���ϸ����time,��������Ҫ��������,��Ϊ�����ٶε�ʱ�� */
            if (i == inc_acc_stp - 1)
            {
                sum_t = fabs(sum_t - time);
            }
        }
        /* �����ٶ��ٶȼ��� */
        else
        {
            sum_t += ti;                                                /* ʱ���ۼ� */
            delta_v         = 0.5f * jerk * pow(fabs(time - sum_t), 2); /* dV = 1/2 * jerk *(T-t)^2 ��������򿴼����ٵ�ͼ */
            velocity_tab[i] = g_calc_t.vt - delta_v;                    /* V = vt - delta_v */
            if (velocity_tab[i] >= g_calc_t.vt)
            {
                accel_step = i;
                break;
            }
        }
    }
    if (is_dec == true) /* ���� */
    {
        float tmp_Speed = 0;
        /* �������� */
        for (i = 0; i < (accel_step / 2); i++)
        {
            tmp_Speed                        = velocity_tab[i];
            velocity_tab[i]                  = velocity_tab[accel_step - 1 - i]; /* ͷβ�ٶȶԻ� */
            velocity_tab[accel_step - 1 - i] = tmp_Speed;
        }

        g_calc_t.decel_tab  = velocity_tab; /* ���ٶ��ٶȱ� */
        g_calc_t.decel_step = accel_step;   /* ���ٶε��ܲ��� */
    }
    else /* ���� */
    {
        g_calc_t.accel_tab  = velocity_tab; /* ���ٶ��ٶȱ� */
        g_calc_t.accel_step = accel_step;   /* ���ٶε��ܲ��� */
    }

    free(velocity_tab);

    return true;
}

/**
 * @brief       S�ͼӼ����˶�
 * @param       vo:���ٶ�;vt:ĩ�ٶ�;AcTime:����ʱ��;DeTime:����ʱ��;step:����;
 * @retval      ��
 */
void stepmotor_move_rel(int32_t vo, int32_t vt, float AcTime, float DeTime, int32_t step)
{
    if (calc_speed(vo, vt, AcTime) == false) /* ��������ٶε��ٶȺͲ��� */
    {
        return;
    }
    if (calc_speed(vt, vo, DeTime) == false) /* ��������ٶε��ٶȺͲ��� */
    {
        return;
    }

    if (step < 0)
    {
        step = -step;
        ST_DIR(CCW);
    }
    else
    {
        ST_DIR(CW);
    }

    if (step >= (g_calc_t.decel_step + g_calc_t.accel_step)) /* ���ܲ������ڵ��ڼӼ��ٶȲ������ʱ���ſ���ʵ��������S�μӼ��� */
    {
        g_calc_t.step      = step;
        g_calc_t.dec_point = g_calc_t.step - g_calc_t.decel_step; /* ��ʼ���ٵĲ��� */
    }
    else /* ���������Խ����㹻�ļӼ��� */
    {
        /* �������㲻�����˶���Ҫ��ǰ��������ٶȱ���ռ�ڴ��ͷţ��Ա�������ظ����� */
        free(g_calc_t.accel_tab); /* �ͷż��ٶ��ٶȱ� */
        free(g_calc_t.decel_tab); /* �ͷż��ٶ��ٶȱ� */
        printf("�������㣬�������ô���!\r\n");
        return;
    }
    g_calc_t.step_pos = 0;
    g_motor_sta       = STATE_ACCEL; /* ���Ϊ����״̬ */

    g_calc_t.ptr   = g_calc_t.accel_tab; /* �Ѽ��ٶε��ٶȱ�洢��ptr��� */
    g_toggle_pulse = (uint32_t)(T1_FREQ / (*g_calc_t.ptr));
    g_calc_t.ptr++;
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint16_t)(g_toggle_pulse / 2)); /*  ���ö�ʱ���Ƚ�ֵ */
    HAL_TIM_OC_Start_IT(&htim1, TIM_CHANNEL_1);                                   /* ʹ�ܶ�ʱ��ͨ�� */
    ST_EN(EN_ON);
}

/**
 * @brief  ��ʱ���Ƚ��ж�
 * @param  htim����ʱ�����ָ��
 * @note   ��
 * @retval ��
 */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim)
{
    volatile uint32_t       Tim_Count = 0;
    volatile uint32_t       tmp       = 0;
    volatile float          Tim_Pulse = 0;
    volatile static uint8_t i         = 0;

    {
        i++;        /* ��ʱ���жϴ�������ֵ */
        if (i == 2) /* 2�Σ�˵���Ѿ����һ���������� */
        {
            i = 0;        /* ���㶨ʱ���жϴ�������ֵ */
            g_step_pos++; /* ��ǰλ�� */
            if ((g_motor_sta != STATE_IDLE) && (g_motor_sta != STATE_STOP))
            {
                g_calc_t.step_pos++;
            }
            switch (g_motor_sta)
            {
                case STATE_ACCEL:
                    g_add_pulse_count++;
                    Tim_Pulse = T1_FREQ / (*g_calc_t.ptr);        /* ���ٶȱ�õ�ÿһ���Ķ�ʱ������ֵ */
                    g_calc_t.ptr++;                               /* ȡ�ٶȱ����һλ */
                    g_toggle_pulse = (uint16_t)(Tim_Pulse / 2);   /* ��תģʽC��Ҫ����2 */
                    if (g_calc_t.step_pos >= g_calc_t.accel_step) /* �����ڼ��ٶβ����ͽ������� */
                    {
                        free(g_calc_t.accel_tab); /* �˶���Ҫ�ͷ��ڴ� */
                        g_motor_sta = STATE_AVESPEED;
                    }
                    break;
                case STATE_DECEL:
                    g_add_pulse_count++;
                    Tim_Pulse = T1_FREQ / (*g_calc_t.ptr); /* ���ٶȱ�õ�ÿһ���Ķ�ʱ������ֵ */
                    g_calc_t.ptr++;
                    g_toggle_pulse = (uint16_t)(Tim_Pulse / 2);
                    if (g_calc_t.step_pos >= g_calc_t.step)
                    {
                        free(g_calc_t.decel_tab); /* �˶���Ҫ�ͷ��ڴ� */
                        g_motor_sta = STATE_STOP;
                    }
                    break;
                case STATE_AVESPEED:
                    g_add_pulse_count++;
                    Tim_Pulse      = T1_FREQ / g_calc_t.vt;
                    g_toggle_pulse = (uint16_t)(Tim_Pulse / 2);
                    if (g_calc_t.step_pos >= g_calc_t.dec_point)
                    {
                        g_calc_t.ptr = g_calc_t.decel_tab; /* �����ٶε��ٶȱ�ֵ��ptr */
                        g_motor_sta  = STATE_DECEL;
                    }
                    break;
                case STATE_STOP:
                    HAL_TIM_OC_Stop_IT(&htim1, TIM_CHANNEL_1); /* ������ӦPWMͨ�� */
                    g_motor_sta = STATE_IDLE;
                    break;
                case STATE_IDLE:
                    break;
            }
        }
        /*  ���ñȽ�ֵ */
        Tim_Count = __HAL_TIM_GET_COUNTER(&htim1);
        tmp       = 0xFFFF & (Tim_Count + g_toggle_pulse);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, tmp);
    }
}

#elif DEMO == 3

/**
 * @brief       �޸Ķ�ʱ��Ƶ�ʽ������ò�������ٶ�
 * @param       speed   : ��ʱ������װ��ֵ
 * @param       Channel : ��ʱ��ͨ��
 * @retval      ��
 */
void stepper_pwmt_speed(uint16_t speed, uint32_t Channel)
{
    __HAL_TIM_SetAutoreload(&htim1, speed);
    __HAL_TIM_SetCompare(&htim1, Channel, __HAL_TIM_GET_AUTORELOAD(&htim1) >> 1);
}

/*****************************************************ֱ�߲岹ʵ��*****************************************************/

inter_pol_def g_pol_par = {0}; /* ֱ�߲岹����ֵ */

const st_motor_ctr_def st_motor[2] =
    {
        {STEPMOTOR_TIM_PULSE_PIN_X,
         STEPMOTOR_TIM_CHANNEL1,
         STEPMOTOR_X_ENA_PIN,
         STEPMOTOR_X_DIR_PIN,
         STEPMOTOR_X_DIR_PORT,
         STEPMOTOR_X_ENA_PORT},
        {STEPMOTOR_TIM_PULSE_PIN_Y,
         STEPMOTOR_TIM_CHANNEL2,
         STEPMOTOR_Y_ENA_PIN,
         STEPMOTOR_Y_DIR_PIN,
         STEPMOTOR_Y_DIR_PORT,
         STEPMOTOR_Y_ENA_PORT},
};
__IO st_motor_status_def g_motor_sta = STATE_STOP; /* ��������˶�״̬ */

/**
 * @brief       ���������������ʱ����ʼ��
 * @param       ��
 * @retval      ��
 */
void stepmotor_init(void)
{
    __HAL_TIM_MOE_ENABLE(&htim1);              /* �����ʹ�� */
    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE); /* ��������жϱ�־λ */
}

/**
 * @brief       ֱ�������岹����ʵ��ֱ�߲岹����,������������ֱ���X���Y�Ჽ��IncX,IncY��
 * @param       IncX    ���յ�X������
 * @param       IncY    ���յ�Y������
 * @param       Speed   �������ٶ�
 * @retval      ��
 */
void line_incmove(uint32_t IncX, uint32_t IncY, uint32_t Speed)
{
    /* ƫ������� */
    g_pol_par.f_e = 0;

    /* ������㵽�յ������Ӧ��������λ��*/
    g_pol_par.end_x     = IncX;
    g_pol_par.end_y     = IncY;
    g_pol_par.end_pulse = g_pol_par.end_y + g_pol_par.end_x;

    /* �����յ��ж���ֱ���ϵĽ�������,����ƫ�� */
    if (g_pol_par.end_y > g_pol_par.end_x)
    {
        g_pol_par.act_axis = AXIS_Y; /* ��һ������Y�� */
        g_pol_par.f_e      = g_pol_par.f_e + g_pol_par.end_x;
    }
    else
    {
        g_pol_par.act_axis = AXIS_X; /* ��һ������X�� */
        g_pol_par.f_e      = g_pol_par.f_e - g_pol_par.end_y;
    }
    /* ����ͨ���ıȽ�ֵ */
    __HAL_TIM_SET_COMPARE(&htim1, st_motor[AXIS_X].pulse_channel, Speed);
    __HAL_TIM_SET_COMPARE(&htim1, st_motor[AXIS_Y].pulse_channel, Speed);
    __HAL_TIM_SET_AUTORELOAD(&htim1, Speed * 2); /* ARR����Ϊ�Ƚ�ֵ2������������Ĳ��ξ���50%��ռ�ձ� */

    TIM_CCxChannelCmd(TIM8, st_motor[g_pol_par.act_axis].pulse_channel, TIM_CCx_ENABLE); /* ʹ��ͨ����� */
    HAL_TIM_Base_Start_IT(&htim1);                                                       /* ʹ�ܶ�ʱ���Լ����������ж� */
    g_motor_sta = STATE_RUN;                                                             /* ��ǵ�������˶� */
}
/**
 * @brief       ʵ����������ֱ�߲岹
 * @param       coordsX    ���յ�X������
 * @param       coordsY    ���յ�Y������
 * @param       Speed      �������ٶ�
 * @retval      ��
 */
void line_inpolation(int32_t coordsX, int32_t coordsY, int32_t Speed)
{
    if (g_motor_sta != STATE_STOP) /* ��ǰ���������ת */
    {
        return;
    }
    /* �������޵�ֱ�߸���һ������һ��,ֻ�ǵ���˶�����һ�� */
    g_pol_par.moving_mode = LINE;
    if (coordsX < 0) /* ��x��С��0ʱ�����������Ϊ����*/
    {
        g_pol_par.x_dir = CCW;
        coordsX         = -coordsX; /* ȡ����ֵ */
        ST_LINE_DIR(CCW, AXIS_X);
    }
    else
    {
        g_pol_par.x_dir = CW;
        ST_LINE_DIR(CW, AXIS_X);
    }
    if (coordsY < 0) /* ��y��С��0ʱ�����������Ϊ����*/
    {
        g_pol_par.y_dir = CCW;
        coordsY         = -coordsY; /* ȡ����ֵ */
        ST_LINE_DIR(CCW, AXIS_Y);
    }
    else
    {
        g_pol_par.y_dir = CW;
        ST_LINE_DIR(CW, AXIS_Y);
    }
    line_incmove(coordsX, coordsY, Speed);
}

/**
 * @brief       ��ʱ���жϻص�����
 * @param       htim �� ��ʱ�����
 * @retval      ��
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    __IO uint32_t axis = 0;                  /* ������ */
    axis               = g_pol_par.act_axis; /* ��ǰ������ */

    /* �ж��Ƿ񵽴��յ���߻�û��ʼ�˶� */
    if (g_pol_par.end_pulse == 0)
    {
        return;
    }
    /* ���ݽ������� ��������ֵ */

    if (g_pol_par.moving_mode == LINE)
    {
        if (g_pol_par.f_e > 0) /* ƫ��� > 0 ,˵����ǰλ��λ��ֱ���Ϸ�,Ӧ��X����� */
        {
            g_pol_par.act_axis = AXIS_X;
            g_pol_par.f_e      = g_pol_par.f_e - g_pol_par.end_y; /* ��һ���޵�X�����ʱ,ƫ����� */
        }
        else if (g_pol_par.f_e < 0) /* ƫ��� < 0 ,˵����ǰλ��λ��ֱ���·�,Ӧ��Y����� */
        {
            g_pol_par.act_axis = AXIS_Y;
            g_pol_par.f_e      = g_pol_par.f_e + g_pol_par.end_x; /* ��һ���޵�Y�����ʱ,ƫ����� */
        }
        /* ƫ��Ϊ0��ʱ��,�ж�x,y���յ�Ĵ�С������������ */
        else if (g_pol_par.f_e == 0) /* ƫ��� = 0 ,˵����ǰλ��λ��ֱ��,Ӧ�ж��յ������ٽ��� */
        {
            if (g_pol_par.end_y > g_pol_par.end_x) /* ��Y������Ļ���Ӧ��Y����� */
            {
                g_pol_par.act_axis = AXIS_Y;
                g_pol_par.f_e      = g_pol_par.f_e + g_pol_par.end_x; /* ��һ���޵�Y�����ʱ,ƫ����� */
            }
            else
            {
                g_pol_par.act_axis = AXIS_X;
                g_pol_par.f_e      = g_pol_par.f_e - g_pol_par.end_y;
            }
        }
    }
    /* �ж��Ƿ���Ҫ���������� */
    if (axis != g_pol_par.act_axis)
    {
        TIM_CCxChannelCmd(TIM8, st_motor[axis].pulse_channel, TIM_CCx_DISABLE);
        TIM_CCxChannelCmd(TIM8, st_motor[g_pol_par.act_axis].pulse_channel, TIM_CCx_ENABLE);
    }
    /* �յ��б�:�ܲ��� */
    g_pol_par.end_pulse--;
    if (g_pol_par.end_pulse == 0)
    {
        g_motor_sta = STATE_STOP;                                                             /* �����յ� */
        TIM_CCxChannelCmd(TIM8, st_motor[g_pol_par.act_axis].pulse_channel, TIM_CCx_DISABLE); /* �رյ�ǰ����� */
        HAL_TIM_Base_Stop_IT(&htim1);                                                         /* ֹͣ��ʱ�� */
    }
}

#endif
