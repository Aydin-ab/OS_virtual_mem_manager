mmy: mmu.cpp
	bash -c "module load gcc-9.2"
	g++ -std=c++11 -g mmu.cpp -o mmu

clean:
	rm -f mmu *~