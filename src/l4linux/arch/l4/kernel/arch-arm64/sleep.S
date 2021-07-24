
.global cpu_resume
.global __cpu_suspend_enter
__cpu_suspend_enter:
cpu_resume:
	1: b 1b
