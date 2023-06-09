/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis_power_control.c/h
  * @brief      chassis power control.���̹��ʿ���
  * @note       this is only controling 80 w power, mainly limit motor current set.
  *             if power limit is 40w, reduce the value JUDGE_TOTAL_CURRENT_LIMIT 
  *             and POWER_CURRENT_LIMIT, and chassis max speed (include max_vx_speed, min_vx_speed)
  *             ֻ����80w���ʣ���Ҫͨ�����Ƶ�������趨�?,������ƹ�����?40w������
  *             JUDGE_TOTAL_CURRENT_LIMIT��POWER_CURRENT_LIMIT��ֵ�����е�������ٶ�?
  *             (����max_vx_speed, min_vx_speed)
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Nov-11-2019     RM              1. add chassis power control
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#include "chassis_power_control.h"
#include "referee.h"
#include "arm_math.h"
#include "detect_task.h"

//���������궨��ֻ�ܿ�һ��
#define CHASSIS_POWER_CONTROL_OFFICIAL //�򿪹ٴ����̹��ʿ��ƺ���
//#define CHASSIS_POWER_CONTROL_NO_POWER_BUFF //��ʹ�ó������� ���̹��ʿ��ƺ���
//#define CHASSIS_POWER_CONTROL_POWER_BUFF //ʹ�ó������� ���̹��ʿ��ƺ���

//#define CHASSIS_POWER_CONTROL_BLOOD_FIRST_POWER_1 55 //һ��Ѫ������ ��������
//#define CHASSIS_POWER_CONTROL_BLOOD_FIRST_POWER_2 60 //����Ѫ������ ��������
//#define CHASSIS_POWER_CONTROL_BLOOD_FIRST_POWER_3 65 //����Ѫ������ ��������

//#define CHASSIS_POWER_CONTROL_POWER_FIRST_POWER_1 70 //һ���������� ��������
//#define CHASSIS_POWER_CONTROL_POWER_FIRST_POWER_2 90 //������������ ��������
//#define CHASSIS_POWER_CONTROL_POWER_FIRST_POWER_3 120 //������������ ��������




#define POWER_LIMIT         80.0f
uint16_t WARNING_POWER; 
#define WARNING_POWER_BUFF  50.0f   

#define NO_JUDGE_TOTAL_CURRENT_LIMIT    64000.0f    //16000 * 4, 
#define BUFFER_TOTAL_CURRENT_LIMIT      16000.0f    //4000 * 4
#define POWER_TOTAL_CURRENT_LIMIT       20000.0f

/**
  * @brief          limit the power, mainly limit motor current
  * @param[in]      chassis_power_control: chassis data 
  * @retval         none
  */
/**
  * @brief          ���ƹ��ʣ���Ҫ���Ƶ������?
  * @param[in]      chassis_power_control: ��������
  * @retval         none
  */
	
#ifdef CHASSIS_POWER_CONTROL_OFFICIAL
void chassis_power_control(chassis_move_t *chassis_power_control)
{
    fp32 chassis_power = 0.0f;
    fp32 chassis_power_buffer = 0.0f;
    fp32 total_current_limit = 0.0f;
    fp32 total_current = 0.0f;
    uint8_t robot_id = get_robot_id();
    uint8_t robot_level = get_robot_level();
    WARNING_POWER = 50;
    if(robot_level == 1)
    {
        WARNING_POWER = 55;
    }
    else if(robot_level == 2)
    {
        WARNING_POWER = 60;
    }
    else if(robot_level == 3)
    {
        WARNING_POWER = 65;
    }
    
    if(toe_is_error(REFEREE_TOE))
    {
        total_current_limit = NO_JUDGE_TOTAL_CURRENT_LIMIT;
    }
    else if(robot_id == RED_ENGINEER || robot_id == BLUE_ENGINEER || robot_id == 0)
    {
        total_current_limit = NO_JUDGE_TOTAL_CURRENT_LIMIT;
    }    
		else
    {
        get_chassis_power_and_buffer(&chassis_power, &chassis_power_buffer);
        // power > 80w and buffer < 60j, because buffer < 60 means power has been more than 80w
        //���ʳ���80w �ͻ�������С��60j,��Ϊ��������С��60��ζ�Ź��ʳ���80w
			
        if(chassis_power_buffer < WARNING_POWER_BUFF)
        {
            fp32 power_scale;
            if(chassis_power_buffer > 5.0f)
            {
                //scale down WARNING_POWER_BUFF
                //��СWARNING_POWER_BUFF
                power_scale = chassis_power_buffer / WARNING_POWER_BUFF;
            }
            else
            {
                //only left 10% of WARNING_POWER_BUFF
                power_scale = 0.0f; //5.0f / WARNING_POWER_BUFF;
            }
            //scale down
            //��С
            total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT * power_scale;
        }
        else
        {
            //power > WARNING_POWER
            //���ʴ���WARNING_POWER
            if(chassis_power > WARNING_POWER)
            {
                fp32 power_scale;
                //power < 80w
                //����С��80w
                if(chassis_power < POWER_LIMIT)
                {
                    //scale down
                    //��С
                      power_scale = (POWER_LIMIT - chassis_power) / (POWER_LIMIT - WARNING_POWER);
                    
                }
                //power > 80w
                //���ʴ���80w
                else
                {
                    power_scale = 0.0f;
                }
                
                total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT + POWER_TOTAL_CURRENT_LIMIT * power_scale;
            }
            //power < WARNING_POWER
            //����С��WARNING_POWER
            else
            {
                total_current_limit = BUFFER_TOTAL_CURRENT_LIMIT + POWER_TOTAL_CURRENT_LIMIT;
            }
        }
    }

    
    total_current = 0.0f;
    //calculate the original motor current set
    //����ԭ����������趨
    for(uint8_t i = 0; i < 4; i++)
    {
        total_current += fabs(chassis_power_control->motor_speed_pid[i].out);
    }
    

    if(total_current > total_current_limit)
    {
        fp32 current_scale = total_current_limit / total_current;
        chassis_power_control->motor_speed_pid[0].out*=current_scale;
        chassis_power_control->motor_speed_pid[1].out*=current_scale;
        chassis_power_control->motor_speed_pid[2].out*=current_scale;
        chassis_power_control->motor_speed_pid[3].out*=current_scale;
    }
}
#endif
#ifdef CHASSIS_POWER_CONTROL_NO_POWER_BUFF
void chassis_power_control(chassis_move_t *chassis_power_control)
{
    fp32 chassis_power = 0.0f;
    fp32 chassis_power_buffer = 0.0f;
    fp32 total_current_limit = 0.0f;
    fp32 total_current = 0.0f;
    uint8_t robot_id = get_robot_id();
    uint8_t robot_level = get_robot_level();
		WARNING_POWER = 55;
    WARNING_POWER = get_robot_chassis_power_limit();
		fp32 current_power, current_buffer, power_scale;
		get_chassis_power_and_buffer(&current_power, &current_buffer);
    if(toe_is_error(REFEREE_TOE))
    {
        total_current_limit = NO_JUDGE_TOTAL_CURRENT_LIMIT;
    }
    else if(robot_id == RED_ENGINEER || robot_id == BLUE_ENGINEER || robot_id == 0)
    {
        total_current_limit = NO_JUDGE_TOTAL_CURRENT_LIMIT;
    }
		else
		{
			if(current_power >= WARNING_POWER)
			{
				power_scale = WARNING_POWER / current_power;
				usart_printf("%f\r\n", power_scale);
			
			
			
				total_current_limit = POWER_TOTAL_CURRENT_LIMIT * power_scale;
				
				total_current = 0.0f;
				//calculate the original motor current set
				//����ԭ����������趨
				for(uint8_t i = 0; i < 4; i++)
				{
						total_current += fabs(chassis_power_control->motor_speed_pid[i].out);
				}
				

				if(total_current > total_current_limit)
				{
						fp32 current_scale = total_current_limit / total_current;
						chassis_power_control->motor_speed_pid[0].out*=current_scale;
						chassis_power_control->motor_speed_pid[1].out*=current_scale;
						chassis_power_control->motor_speed_pid[2].out*=current_scale;
						chassis_power_control->motor_speed_pid[3].out*=current_scale;
						
				}
			}
		}
}
#endif
#ifdef CHASSIS_POWER_CONTROL_POWER_BUFF

#endif
