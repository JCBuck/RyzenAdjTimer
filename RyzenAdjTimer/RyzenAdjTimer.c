#include <stdio.h>
#include <stdint.h>
#include "ryzenadj.h"
#include "windows.h"
#include <stdbool.h>
#pragma comment(lib,"libryzenadj.lib")

#define MAXTDP 100*1000

//ported from powershell, @prj https://github.com/FlyGoat/RyzenAdj/issues/145#issuecomment-840856670


//float stapm_hysteresis = 1.0f, stapm_reset_limit = 10000.0f, stapm_time_target = 100.0f, stapm_normal_limit = 15000.0f;
//float stapm_hysteresis = 1.0f, stapm_reset_limit = 10*1000.0f, stapm_time_target = 100.0f, stapm_normal_limit = 100*1000.0f;
//Thinkpad L15 Gen 1, 4750u, seemed to be able to hit 70+ W on package power
//Thinkpad L15 Gen 2, 5850u, bios 1.20 only allows 25w stapm, and slow limit of 37.5w, fast limit of 48w, slow time of 30s
//Thinkpad T14 Gen 2 20XK-0015US, 5650u bios 1.15 seems to be similar to L15 Gen 2
float stapm_hysteresis = 1.0f; 
uint32_t stapm_reset_limit = 20 * 1000, stapm_time_target = 500, stapm_normal_limit = MAXTDP;
float slow_hysteresis = 1.0f; 
uint32_t slow_reset_limit = 32 * 1000, slow_time_target = 500, slow_normal_limit = MAXTDP;
uint32_t fast_normal_limit = MAXTDP;
uint32_t tctl_temp = 100, vrmmax_current = 100 * 1000;

uint32_t debuglevel = 4; // default to show reset status, 0 to turn off
uint32_t sleepms = 12; //experimentally shown to max out to fast on AMD thinkpads
uint32_t statusdelay = 970; //approx 1 second
uint32_t titlestatus = 0;
uint32_t dispstatus = 0;
uint32_t hotkeys = 1;

void bypass_limits(ryzen_access ry) {


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

	//if(debuglevel & 4) printf("[STAPM_RESET] Limit: %f Value: %f  SL: %f SV: %f FL: %f FV: %f @ %u\n", cur_limit, cur_value, cur_limit_slow, cur_value_slow, cur_limit_fast, cur_value_fast, GetTickCount64());

	if (cur_value > (cur_limit - stapm_hysteresis)) {
		if(debuglevel & 4) printf("[STAPM_RESET] stapm_value (%f) nearing stapm_limit (%f), resetting...\n",cur_value,cur_limit);
		result =      set_stapm_limit(ry, stapm_reset_limit);
		result =      set_stapm_time(ry, 0);
		while (true) {
			Sleep(sleepms);
			refresh_table(ry);
			cur_limit =      get_stapm_limit(ry);
			cur_value =      get_stapm_value(ry);
			if(debuglevel & 4) printf("[STAPM_RESET] iter: %d stapm_value: %f\n",iters,cur_value);
			if (cur_limit != (stapm_reset_limit / 1000.0f)) {
				if(debuglevel & 4) printf("[STAPM_RESET] Reset failed, something else changed stapm_limit.\n");
				break;
			}
			if (cur_value < ((stapm_reset_limit / 1000.0f) + stapm_hysteresis)) {
				if(debuglevel & 4) printf("[STAPM_RESET] Reset successful, stapm_value now at %f, needed %d iterations.\n",cur_value,iters);
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
		if(debuglevel & 4) printf("[SLOW_RESET] slow_value (%f) nearing slow_limit (%f), resetting...\n", cur_value_slow, cur_limit_slow);
		result = set_slow_limit(ry, slow_reset_limit);
		result = set_slow_time(ry, 0);
		while (true) {
			Sleep(sleepms);
			refresh_table(ry);
			cur_limit_slow = get_slow_limit(ry);
			cur_value_slow = get_slow_value(ry);
			if(debuglevel & 4) printf("[SLOW_RESET] iter: %d slow_value: %f\n", iters, cur_value_slow);
			if (cur_limit_slow != (slow_reset_limit / 1000.0f)) {
				if(debuglevel & 4) printf("[SLOW_RESET] Reset failed, something else changed slow_limit.\n");
				break;
			}
			if (cur_value_slow < ((slow_reset_limit / 1000.0f) + slow_hysteresis)) {
				if(debuglevel & 4) printf("[SLOW_RESET] Reset successful, slow_value now at %f, needed %d iterations.\n", cur_value_slow, iters);
				break;
			}
			iters++;
		}
		result = set_slow_time(ry, slow_time_target);
		result = set_slow_limit(ry, slow_normal_limit);
		refresh_table(ry);
	}
}
typedef struct param {
	char argshort[32];
	char arglong[32];
	uint32_t * dest; //64-bit ptr
	int (*setfn)(ryzen_access, uint32_t); //64-bit ptr
} sparam;

//uint32_t resetvalbitmask = 0x01F4; // 1 1111 0100
//uint32_t resetvalbitmask = 0x4; // 0 0000 0100
uint32_t resetvalbitmask = 1; // 0 0000 0000
sparam arr_parms[] = {
	{"-a","--stapm_normal_limit", &stapm_normal_limit, set_stapm_limit },// 1->1 seems to be req to hit fast limits on thinkpads
	{"-b","--fast_normal_limit", &fast_normal_limit, set_fast_limit},    // 0->2
	{"-c","--slow_normal_limit", &slow_normal_limit, set_slow_limit},    // 0->4
	{"-d","--slow_time_target", &slow_time_target, set_slow_time},       // 0->8

	{"-e","--stapm_time_target", &stapm_time_target, set_stapm_time },   // 0->1
	{"-f","--tctl_temp", &tctl_temp, set_tctl_temp},                     // 0->2
	{"-g","--vrmmax_current", &vrmmax_current, set_vrmmax_current},      // 0->4
	{"-h","--slow_reset_limit", &slow_reset_limit, NULL},                // 0->8 used by limit bypass algo

	{"-i","--stapm_reset_limit", &stapm_reset_limit, NULL },             // 0->1 used by limit bypass algo 



	//implicitly NULL setfn's
	{"-D","--debuglevel", &debuglevel},
	{"-S","--sleepms", &sleepms},
	{"-E","--statusdelay", &statusdelay},
	{"-T","--dispstatus", &dispstatus},
	{"-I","--titlestatus", &titlestatus},
	{"-H","--hotkeys", &hotkeys},
	{"-R","--resetvalbitmask", &resetvalbitmask},
};

int arr_parms_len = sizeof(arr_parms) / sizeof(sparam);
void printvars() {
	printf("[Q]-Quit [Backspace]-Show vals in title [DEL]-Print vals [INS]-Print args\n");
	if (debuglevel & 1) printf("Total parms: %d\n", arr_parms_len);
	for (int i = 0; i < arr_parms_len; i++) {

		printf("[%2d] %s (setfn:%s) %s %-20s %d (0x%x)\n",
			i,
			(resetvalbitmask >> i) & 1?"(1:reset)":"(0:init) ",
			arr_parms[i].setfn?"1":"0",
			arr_parms[i].argshort,
			arr_parms[i].arglong,
			*arr_parms[i].dest,
			*arr_parms[i].dest);
	}
}
int parseargs(int argc, char** argv, char** env) {
	int targetparm = -1;
	if (debuglevel & 1)
		printf("argc = %d\n", argc);
	for (int i = 1; i < argc; i++) {
		if (targetparm >= 0) {
			//best practice for parsing number params https://stackoverflow.com/a/1640804
			char* end;
			long value = strtol(argv[i], &end, 0);
			if (end == argv[i] || *end != '\0' || errno == ERANGE || value < 0) {

				fprintf(stderr, "Error! Expected positive number value for argument: %s\n", argv[i - 1]);
				return -1;
			}

			*arr_parms[targetparm].dest = value;
			targetparm = -1;
		}
		else {

			for (int a = 0; a < arr_parms_len; a++) {
				if (!strncmp(argv[i], arr_parms[a].arglong, sizeof(arr_parms[0].arglong))) {
					if (debuglevel & 1) printf("Found %d, arglong %s!\n", a, argv[i]);
					targetparm = a;
				}
				else if (!strncmp(argv[i], arr_parms[a].argshort, sizeof(arr_parms[0].argshort))) {
					if (debuglevel & 1) printf("Found %d, argshort %s!\n", a, argv[i]);
					targetparm = a;
				}
				
			}
			if (targetparm == -1) {
				fprintf(stderr, "Error! Invalid argument: %s\n", argv[i]);
				return -1;
			}
		}
	}
	if (targetparm != -1) {
		fprintf(stderr, "Error! Expected positive number value for argument: %s\n", argv[argc-1]);
		return -1;
	}
	printvars();
	return 0;
}
void setargs(ryzen_access ryzenref, int init) {
	for (int i = 0; i < arr_parms_len; i++) { //only first 32
		if ((resetvalbitmask >> i) & 1 || init) {

			if (arr_parms[i].setfn) {
				if (debuglevel & 2) {
					printf("%s %d ", arr_parms[i].arglong, *arr_parms[i].dest);
				}
				if (arr_parms[i].setfn(ryzenref, *arr_parms[i].dest))
					fprintf(stderr, "Failed setting %s\n", arr_parms[i].arglong);
			}
			
		}
	}
	if (debuglevel & 2) {
		printf("\n");
	}
}
int main(int argc, char **argv, char **env)
{
	if (parseargs(argc, argv, env)) {
		printvars();
		return -1;
	}
   SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS); //for tighter sleep granularity
   ryzen_access ryzenref;
   ryzenref = init_ryzenadj();
   init_table(ryzenref);
   refresh_table(ryzenref);

   /*
   %~dp0\ryzenadj.exe --tctl-temp=95 --stapm-limit=90000 --fast-limit=90000 --stapm-time=3600 --slow-limit=90000 --slow-time=3600 --vrmmax-current=100000
   */
   char titlebuf[128]="";
   uint64_t count = 0;
   uint64_t ticks = GetTickCount64();
   printf("Running ryzenadj\n");

   //Sometimes values don't set.. try retrying
   
	   setargs(ryzenref, 1);
   

   while (true) {

	   //Thinkpad firmware seems to almost immediately reset stapm to 15 (and other power levels), need to continually set higher limits
	   //if (set_tctl_temp(ryzenref, 100)) printf("Failed set_tctl_temp\n");
	   //if (set_stapm_limit(ryzenref, MAXTDP)) printf("Failed set_stapm_limit\n");
	   //if (set_fast_limit(ryzenref, MAXTDP)) printf("Failed set_fast_limit\n");
	   ////if(set_stapm_time(ryzenref, 3600*24*10)) printf("Failed set_stapm_time\n");
	   //if (set_slow_limit(ryzenref, MAXTDP)) printf("Failed set_slow_limit\n");
	   //if (set_slow_time(ryzenref, 500/*3600*24*10*/)) printf("Failed set_slow_time\n");
	   //if (set_vrmmax_current(ryzenref, 100000)) printf("Failed set_vrmmax_current\n");
	   //if (set_skin_temp_power_limit(ryzenref, 100000)) printf("Failed set_skin_temp_power_limit\n");
	   
	   //reset all based on bitmask
	   setargs(ryzenref,0);

	   if (dispstatus || titlestatus) {
		   if (GetTickCount64() - ticks > statusdelay) { //about a second
			   ticks = GetTickCount64();



			   if (dispstatus) {
				   printf("stapm val: % 6.2f/% 6.2f fast: % 6.2f/% 6.2f slow:% 6.2f/% 6.2f\n",
					   get_stapm_value(ryzenref), get_stapm_limit(ryzenref),
					   get_fast_value(ryzenref), get_fast_limit(ryzenref),
					   get_slow_value(ryzenref), get_slow_limit(ryzenref));
			   }
			   if (titlestatus) {
				   //_ui64toa_s(count++, titlebuf, sizeof(titlebuf), 10);
				   sprintf_s(titlebuf,sizeof(titlebuf),"stapm val: % 6.2f/% 6.2f fast: % 6.2f/% 6.2f slow:% 6.2f/% 6.2f\n",
					   get_stapm_value(ryzenref), get_stapm_limit(ryzenref),
					   get_fast_value(ryzenref), get_fast_limit(ryzenref),
					   get_slow_value(ryzenref), get_slow_limit(ryzenref));
				   SetConsoleTitleA(titlebuf);
			   }
		   }

	   }

	   //VK_RETURN would capture pressing enter in terminal on first execution so... using something else
	   if (hotkeys) {
		   if (GetAsyncKeyState(VK_DELETE) & 0x0001) {
			   printf("Dispstatus = %d\n", dispstatus = !dispstatus);
		   }
		   if (GetAsyncKeyState(VK_BACK) & 0x0001) {
			   printf("titlestatus = %d\n", titlestatus = !titlestatus);
			   if (!titlestatus) {
				   //strcat_s(titlebuf, sizeof(titlebuf), " [PAUSED]");
				   //sprintf_s(titlebuf,sizeof(titlebuf),"%s [PAUSED]");
				   SetConsoleTitleA("[PAUSED]");
			   }
		   }
		   if (GetAsyncKeyState('Q') & 0x0001) {
			   return 0;
		   }
		   if (GetAsyncKeyState(VK_INSERT) & 0x0001) {
			   printvars();
		   }
	   }
	   bypass_limits(ryzenref);
       Sleep(sleepms);
   }
    return 0;
}

