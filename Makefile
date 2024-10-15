GMON_OUT_PREFIX=~/cp/tp/code/profiling/gmon.out;

CPP = g++ -Wall -Ofast 
SRCS = main.cpp fluid_solver.cpp EventManager.cpp

all:
	$(CPP) $(SRCS) -o ~/cp/tp/code/fluid_sim

clean:
	@echo Cleaning up...
	@rm -rf fluid_sim ~/cp/tp/code/profiling/* gmon.out
	@echo Done.

exec: fluid_sim
	srun --partition=cpar ./fluid_sim

profile: fluid_sim
	srun --exclusive --mem=0 --partition=cpar perf stat -e instructions,cycles,L1-dcache-load-misses,L1-dcache-loads,L1-dcache-store-misses,L1-dcache-stores -M cpi ~/cp/tp/code/fluid_sim
#	srun --partition=cpar gprof fluid_sim gmon.out > ~/cp/tp/code/profiling/main.gprof

profile-all: fluid_sim
	srun --partition=cpar perf stat ~/cp/tp/code/fluid_sim
