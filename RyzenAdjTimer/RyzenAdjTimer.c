#include <stdio.h>
#include <stdint.h>
#include "ryzenadj.h"
#include "windows.h"
#include <stdbool.h>
#pragma comment(lib,"libryzenadj.lib")

#define MAXTDP 100*1000

//ported from powershell, @prj https://github.com/FlyGoat/RyzenAdj/issues/145#issuecomment-840856670
void bypass_limits(ryzen_access ry) {


	//float stapm_hysteresis = 1.0f, stapm_reset_limit = 10000.0f, stapm_time_target = 100.0f, stapm_normal_limit = 15000.0f;
	//float stapm_hysteresis = 1.0f, stapm_reset_limit = 10*1000.0f, stapm_time_target = 100.0f, stapm_normal_limit = 100*1000.0f;
	//Thinkpad L15 Gen 2, bios 1.20 only allows 25w stapm, and slow limit of 37.5w, fast limit of 48w, slow time of 30s
	//
	float stapm_hysteresis = 1.0f; uint32_t stapm_reset_limit = 20 * 1000, stapm_time_target = 500, stapm_normal_limit = 100 * 1000;
	float slow_hysteresis = 1.0f; uint32_t slow_reset_limit = 32 * 1000, slow_time_target = 500, slow_normal_limit = 100 * 1000;
	int result = 0;

	refresh_table(ry);

	// Workaround for locked stapm_limit on Cezanne
	float cur_limit =      get_stapm_limit(ry);
	float cur_value =      get_stapm_value(ry);
	float cur_limit_slow = get_slow_limit(ry);
	float cur_value_slow = get_slow_value(ry);
	//float cur_limit_fast = get_fast_limit(ry);
	//float cur_value_fast = get_fast_value(ry);
	int iters = 0;

	//printf("[STAPM_RESET] Limit: %f Value: %f  SL: %f SV: %f FL: %f FV: %f @ %u\n", cur_limit, cur_value, cur_limit_slow, cur_value_slow, cur_limit_fast, cur_value_fast, GetTickCount64());

	if (cur_value > (cur_limit - stapm_hysteresis)) {
		printf("[STAPM_RESET] stapm_value (%f) nearing stapm_limit (%f), resetting...\n",cur_value,cur_limit);
		result =      set_stapm_limit(ry, stapm_reset_limit);
		result =      set_stapm_time(ry, 0);
		while (true) {
			Sleep(10);
			refresh_table(ry);
			cur_limit =      get_stapm_limit(ry);
			cur_value =      get_stapm_value(ry);
			printf("[STAPM_RESET] iter: %d stapm_value: %f\n",iters,cur_value);
			if (cur_limit != (stapm_reset_limit / 1000.0f)) {
				printf("[STAPM_RESET] Reset failed, something else changed stapm_limit.\n");
				break;
			}
			if (cur_value < ((stapm_reset_limit / 1000.0f) + stapm_hysteresis)) {
				printf("[STAPM_RESET] Reset successful, stapm_value now at %f, needed %d iterations.\n",cur_value,iters);
				break;
			}
			iters++;
		}
		result =      set_stapm_time(ry, stapm_time_target);
		result =      set_stapm_limit(ry, stapm_normal_limit);
		refresh_table(ry);
	}
	iters = 0;
	if (cur_value_slow > (cur_limit_slow - slow_hysteresis)) {
		printf("[SLOW_RESET] slow_value (%f) nearing slow_limit (%f), resetting...\n", cur_value_slow, cur_limit_slow);
		result = set_slow_limit(ry, slow_reset_limit);
		result = set_slow_time(ry, 0);
		while (true) {
			Sleep(10);
			refresh_table(ry);
			cur_limit_slow = get_slow_limit(ry);
			cur_value_slow = get_slow_value(ry);
			printf("[SLOW_RESET] iter: %d slow_value: %f\n", iters, cur_value_slow);
			if (cur_limit_slow != (slow_reset_limit / 1000.0f)) {
				printf("[SLOW_RESET] Reset failed, something else changed slow_limit.\n");
				break;
			}
			if (cur_value_slow < ((slow_reset_limit / 1000.0f) + slow_hysteresis)) {
				printf("[SLOW_RESET] Reset successful, slow_value now at %f, needed %d iterations.\n", cur_value_slow, iters);
				break;
			}
			iters++;
		}
		result = set_slow_time(ry, slow_time_target);
		result = set_slow_limit(ry, slow_normal_limit);
		refresh_table(ry);
	}
}
int main(int argc, char **argv, char **env)
{
   SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS); //for tighter sleep granularity
   ryzen_access ryzenref;
   ryzenref = init_ryzenadj();
   init_table(ryzenref);
   refresh_table(ryzenref);

   /*
   %~dp0\ryzenadj.exe --tctl-temp=95 --stapm-limit=90000 --fast-limit=90000 --stapm-time=3600 --slow-limit=90000 --slow-time=3600 --vrmmax-current=100000
   */
   //char titlebuf[32]="";
   //uint64_t count = 0;
   //uint64_t ticks = GetTickCount64();
   printf("Running ryzenadj Thinkpad L15 Gen2 48w max override @ 100hz \n");
   

   while (true) {

	   //Thinkpad firmware seems to almost immediately reset stapm to 15 (and other power levels), need to continually set higher limits
       if(set_tctl_temp(ryzenref, 100)) printf("Failed set_tctl_temp\n");
       if(set_stapm_limit(ryzenref, MAXTDP)) printf("Failed set_stapm_limit\n");
       if(set_fast_limit(ryzenref, MAXTDP)) printf("Failed set_fast_limit\n");
       //if(set_stapm_time(ryzenref, 3600*24*10)) printf("Failed set_stapm_time\n");
       if(set_slow_limit(ryzenref, MAXTDP)) printf("Failed set_slow_limit\n");
       if(set_slow_time(ryzenref, 500/*3600*24*10*/)) printf("Failed set_slow_time\n");
	   if (set_vrmmax_current(ryzenref, 100000)) printf("Failed set_vrmmax_current\n");
	   //if (set_skin_temp_power_limit(ryzenref, 100000)) printf("Failed set_skin_temp_power_limit\n");

       //if (GetTickCount64() - ticks > 970) { //about a second
       //    ticks = GetTickCount64();
       //    //printf("stapm val: % 6.2f fast: % 6.2f slow:% 6.2f\n", get_stapm_value(ryzenref), get_fast_value(ryzenref), get_slow_value(ryzenref));

       //}
       //if (argc > 1) {
       //    _ui64toa_s(count++, titlebuf, sizeof(titlebuf), 10);
       //    SetConsoleTitleA(titlebuf);
       //}

	   bypass_limits(ryzenref);
       Sleep(10);
   }
    return 0;
}

