# SSD-GCN-Project
(2020.02.17)iniReader 완료
txtReader개발중



(2020.03.14) 수정사항

Accelerator.h
	mac_1, mac_2 -> flag.mac_1 , flag.mac_2 로 수정
	149: 변수 address 선언 추가

DRAMInterface.h
	10: #include <DRAMSim2/DRAMSim.h> -> #include <DRAMSim.h> 으로 수정 (링크 관련)
DRAMInterface.cpp
	14: clk_period_in_ns 타입이 다른곳은 전부 float 길래 unit64_t 에서 float 로 변경

Common.h
	12: using namespace std; 추가 (queue 사용)
	11: cassert 헤더 추가
	34: AXBuffer 에서 value의 queue 타입 float 로 변경

BufferInterface.cpp
	90: weightbuffer 의 index 는 포인터이므로 -> 으로 접근해야됨

IniParser.cpp
	33: GetFrequency("ClockPeriod") -> GetClockPeriod("ClockPeriod") 로 변경

SSDGCNSim.cpp, h
	test.cpp 에 적혀있던 RunSimulator() 함수 추가

그외 잡다한 오타 수정

BufferInterface에 XEnd(), AEnd(), IsExist(), Ready() 같은 함수들 아직 추가 안되어있어서 컴파일 불가
전체 파일 완성 후에 make 와 ./result 명령어를 통해 실행가능




(2020.03.15) 수정

BufferInterface.cpp
	GoBack, IsExist(아직 weight 부분만 구현) 함수 추가
	XEnd, AEnd, Ready 더미함수 추가하여 make 실행 가능하게 만듬
	print_status 함수 : buffer 현재 상태 출력

make 명령어 사용 가능
	make -> sim 파일 생성 ./sim 으로 실행 가능

테스트용 main 함수 test.cpp 에 구성하면 됨