GMON_OUT_PREFIX=~/cp/tp/code/profiling/gmon.out;

CPP = g++ -Wall -pg -O2 -fno-omit-frame-pointer
SRCS = main.cpp fluid_solver.cpp EventManager.cpp

all:
	$(CPP) $(SRCS) -o fluid_sim

clean:
	@echo Cleaning up...
	@rm -rf fluid_sim ~/cp/tp/code/profiling/* gmon.out
	@echo Done.

exec: fluid_sim
	srun --partition=cpar ./fluid_sim

profile: fluid_sim
	srun --partition=cpar perf stat -e L1-dcache-load-misses -M cpi ~/cp/tp/code/fluid_sim
	srun --partition=cpar gprof fluid_sim gmon.out; > ~/cp/tp/code/profiling/main.gprof
