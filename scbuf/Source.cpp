#include <windows.h>
#include <conio.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <algorithm>

using namespace std;

#define step 16		//one button size in pixels
#define pause this_thread::sleep_for(1ms);
#define LPUSH mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
#define LPOP mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
#define LCLICK LPUSH LPOP pause
#define RPUSH mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
#define RPOP mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
#define RCLICK RPUSH RPOP pause
#define DCLICK LPUSH RPUSH LPOP RPOP pause 

HWND win;	//handle
HDC dc;		//device context

struct MAP {
	SIZE size{ 0 };
	char **field;
	float freeSquare = 0;
	char num_bombs = 0;
} map;

struct POSITIONSET {
	POINT
		begin_on_screen,	//position of the first pixel regarding
							//top left screen corner (used to set cursor)
		begin_on_field,	//position of the first pixel regarding
						//top left game-window corner (used to take info about squares)
		current_on_screen,	//current cursor position
		current_on_field;	//current square position
} pos;

void scp(POINT p){
	SetCursorPos(p.x, p.y);
}

void startProc() {
	STARTUPINFO st{ 0 };
	PROCESS_INFORMATION pi{ 0 };

	if (!CreateProcess(
		L"E:\\winmine.exe",
		NULL,
		NULL,
		NULL,
		true,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&st,
		&pi
	))
		cout << "There is a problem" << endl;
}

POINT ToScreenPos(short x, short y) {	//translation from field-coordiates 
	return pos.current_on_screen =		//to coordinates for cursor
	{ 
		pos.begin_on_screen.x + step / 2 + x * step, 
		pos.begin_on_screen.y + step / 2 + y * step 
	};
}

POINT ToFieldPos(short x, short y) {	//translation from field-coordiates 
	return pos.current_on_field = 		//to coordinates for getInfo-functions
	{ 
		step / 2 + x * step + pos.begin_on_field.x, 
		step / 2 + y * step + pos.begin_on_field.y 
	};
}

POINT FieldPtoScreenP() {	//translation from field coords to screen coords
	return pos.current_on_screen = 
	{ 
		pos.current_on_field.x + pos.begin_on_screen.x - pos.begin_on_field.x,
		pos.current_on_field.y + pos.begin_on_screen.y - pos.begin_on_field.y 
	};
}

char match(COLORREF color) 
{	//function assotiate colore-code with simple code
	switch (color) {
	case 0xff'00'00:
		return 1;
	case 0x80'00:
		return 2;
	case 0xFF:
		return 3;
	case 0x80'00'00:
		return 4;
	case 0x80:
		return 5;
	case 0x80'80'00:
		return 6;
	case 0x80'80'80:	//free square
		return 0;
	case 0xff'ff'ff:	//hasn't open yet
		return 'X';
	case 0xc0'c0'c0:	//need more inf
		return 'x';
	case 0:
		return 'B';		//Boom(game over)
	default:
		return color;
	}
}

/*
lookAround params:
int i - y-axis coord of the number you will look around	
int j - x-axis coord of the number you will look around
vector<POINT> *round - pointer to the vector of 'param1' buttons around (j, i) 
char param1 - that you want find around (j,i)
*/
short lookAround(int i, int j, vector<POINT> *round, char param1) 
{						/*
						1 2 3
						4 U 5
						6 7 8
						*/
	if (i > 0) 
	{
		if (j > 0 && map.field[i - 1][j - 1] == param1)
			round->push_back({ j - 1, i - 1 });		//1
		if (map.field[i - 1][j] == param1)
			round->push_back({ j, i - 1 });			//2
		if (j < map.size.cx - 1 && map.field[i - 1][j + 1] == param1)
			round->push_back({ j + 1, i - 1 });		//3
	}
	
	if (j > 0 && map.field[i][j - 1] == param1)
		round->push_back({ j - 1, i });				//4
	if (j < map.size.cx - 1 && map.field[i][j + 1] == param1)
		round->push_back({ j + 1, i });				//5
	
	if(i < map.size.cy - 1) 
	{
		if (j > 0 && map.field[i + 1][j - 1] == param1)
			round->push_back({ j - 1, i + 1 });		//6
		if (map.field[i + 1][j] == param1)
			round->push_back({ j, i + 1 });			//7
		if (j < map.size.cx - 1 && map.field[i + 1][j + 1] == param1)
			round->push_back({ j + 1, i + 1 });		//8
	}
	return round->size();
}


/*
первая часть: деактивирует клетки около которых колличество 
неоткрытых полей равно собственному номиналу
*/
bool check(short i, short j) {
	vector<POINT> round, round2;
	bool ret = true;	//default return value
	if (map.field[i][j] > 0 && map.field[i][j] < 9) {	//if number
		round.clear();
		if (lookAround(i, j, &round, 'D') == map.field[i][j])	//'D' == demined
		{				//look for demined points only
			round.clear();
			if (lookAround(i, j, &round, 'X') != 0)
			{		//if there are unknown buttons around
				ret = false;
				scp(ToScreenPos(j, i));
				DCLICK
			}
		}
		else
		{
			if (lookAround(i, j, &round, 'X') == map.field[i][j])
			{							//look for demined points and holls
				ret = false;
				char tmp = map.field[i][j];
				while (tmp--)
				{
					if (map.field[round[tmp].y][round[tmp].x] != 'D')
					{		//neutralization bombs from 'round'
						map.field[round[tmp].y][round[tmp].x] = 'D';
						scp(ToScreenPos(round[tmp].x, round[tmp].y));
						RCLICK
					}
				}
			}
		}
	}
	return ret;
}

//вторая часть: ищет множества полностью включенные в другие множества
//(ищет пересечения неразгаданных клеток относительно цифр)
void checkForCouple(short i, short j, short i2, short j2) {
	vector<POINT> round, round2;
	//набор проверок на допустимые диапазоны
	if (i >= map.size.cy || j >= map.size.cx ||
		i2 >= map.size.cy || j2 >= map.size.cx)
		return;
	if (map.field[i][j] < 1 || map.field[i][j] > 8 ||
		map.field[i2][j2] < 1 || map.field[i2][j2] > 8)
		return;
	
	if (lookAround(i, j, &round, 'X') == 0)
		return;
	if (lookAround(i2, j2, &round2, 'X') == 0)
		return;
	//реверс для большей универсальности алгоритма
	if (round2.size() < round.size()) {
		vector<POINT> tmpv;
		tmpv = round2;
		round2 = round;
		round = tmpv;
	}
	//из большего множества вычетаем меньшее
	for (auto& g = round.begin();;) {
		auto it = find_if(round2.begin(), round2.end(), [&](auto a) { return (a.x == g->x) && (a.y == g->y); });
		if (it == round2.end())
			break;
		else
			round2.erase(it);
		if (++g == round.end()) {
			round.clear();
			break;
		}
	}
	//если одно множество полностью входит в другое
	if (round.size() == 0) {
		auto tmp1 = lookAround(i, j, &round, 'D');
		round.clear();
		auto tmp2 = lookAround(i2, j2, &round, 'D');
		round.clear();
		//если колличество неразгадонных бомб вокруг 2-х точек одинаково,
		//то они находятся на пересечении множеств, а то, что в него не входит
		//можно открывать
		if ((map.field[i][j] - tmp1) == (map.field[i2][j2] - tmp2)) {
			for (auto& g : round2) {
				scp(ToScreenPos(g.x, g.y));
				LCLICK
			}
		}
		else {
			//а если неразгадонных бомб вокруг тех же точек не равное колличество,
			//но их в точности столько, сколько клеток не входит в пересечение большого 
			//и малого множества то в этих клетках и будут бомбы
			if (round2.size() == (map.field[i][j] - tmp1 - map.field[i2][j2] + tmp2) ||
				round2.size() == -(map.field[i][j] - tmp1 - map.field[i2][j2] + tmp2)) {
				for (auto& g : round2) {
					if (map.field[g.y][g.x] != 'D') {
						scp(ToScreenPos(g.x, g.y));
						map.field[g.y][g.x] = 'D';
						RCLICK
					}
				}
			}
		}
	}
}

bool fullCheck() {
	bool ret = true;	//default return value
	for(short num_of_checks = 2; num_of_checks--;)
		for (short i = 0; i < map.size.cy; i++)
			for (short j = 0; j < map.size.cx; j++) 
				switch (num_of_checks) {
				case 1:
					if (!check(i, j))
						ret = false;
					break;
				case 0: //перечислены только те комбинации, 
						//которые попадались на практике
					checkForCouple(i, j, i + 1, j);
					checkForCouple(i, j, i + 2, j);
					checkForCouple(i, j, i + 2, j + 1);
					checkForCouple(i, j, i, j + 1);
					checkForCouple(i, j, i, j + 2);
					break;
				default:
					break;
			}

	return ret;
}


bool ReleaseMap() {
	map.freeSquare = 0;
	for (int i = 0; i < map.size.cy; i++)
		for (int j = 0; j < map.size.cx; j++) {
			ToFieldPos(j, i);
			if (map.field[i][j] != 'D') {	//if square demined
				if (map.field[i][j] != 'X')
					map.freeSquare++;
				map.field[i][j] = match(GetPixel(dc, pos.current_on_field.x, pos.current_on_field.y));
				if (map.field[i][j] == 'x')
					map.field[i][j] = match(GetPixel(dc, pos.current_on_field.x - step / 2, pos.current_on_field.y - step / 2));
				vector<POINT> a;
				if (lookAround(i, j, &a, 'D') == 3 && (j == 0 && i == 0) || (j == map.size.cx && i == 0) || 
					(j == map.size.cx && i == map.size.cy) || (j == 0 && i == map.size.cy))	//if in the corner of the map 4 mines
					map.field[i][j] = 'D';
				a.clear();
				if (map.field[i][j] == 'B')
					return false;
			}
		}
	(map.freeSquare /= map.size.cx * map.size.cy) *= 100;
	return true;
}

void newGame() {
	SetCursorPos(pos.begin_on_screen.x - 5, pos.begin_on_screen.y - 61 );
	LCLICK
	SetCursorPos(pos.begin_on_screen.x + 5, pos.begin_on_screen.y - 51);
	LCLICK
}

void clearMap() {
	for (short a = 0; a < map.size.cy; a++)
		for (short b = 0; b < map.size.cx; b++)
			map.field[a][b] = 'X';
	map.freeSquare = 0;
}

void randStep() {

	short x = 0, y = 0;
	do {
		x = rand() % map.size.cx;
		y = rand() % map.size.cy;
		ToScreenPos(x, y);
	} while (map.field[y][x] != 'X');	//try to step until you find unknown button

	scp(pos.current_on_screen);
	LCLICK

}

void beginGame() {

	short cnt = 0, cntv = 0, cnt1 = 0;
	POINT tmp1{ 0 };
	int is_game_over = 0;	//game over counter
	while (cnt++ < 10000)
	{
		clearMap();	//free field befor you start new game
		this_thread::sleep_for(500ms);
		cout << "Game: " << cnt << '\t';
		while (ReleaseMap())	//Release map befor new step
		{
			if (fullCheck())	//if you check and open or demined smth
			{					//you will not made new random step				
				if(map.freeSquare < 20)	//if there is not enought free square
					randStep();			//we use random
				
				float a = map.num_bombs;
				a /= (map.size.cx * map.size.cy);

				if (map.freeSquare == (100 - a * 100)) {
					cout << "Victory\t";
					cntv++;
					break;
				}

				//IF YOU WANT SEE WHEN MY BOT IS STUPID JUST COMMENT 
				//NEXT "randStep" AND UNCOMMENT "pause"

				if (tmp1.x == pos.current_on_screen.x && tmp1.y == pos.current_on_screen.y) {
					if (cnt1++ == 5) {	//if we can't find normal step we use random again
						randStep();
		//				system("pause");
						cnt1 = 0;
					}
				}
				else {
					tmp1.x = pos.current_on_screen.x;
					tmp1.y = pos.current_on_screen.y;
					cnt1 = 0;
				}
			}
		}
		cout << "Game Over" << endl;
		newGame();
	}
	float a = cntv;
	cout << "Victorys/Games:\t" << cntv << '/' << --cnt << "\t=\t" << (float)(a / cnt);
}

int main() {

	srand(time(NULL));

	startProc();

	cout << "Are you ready, kids?" << endl;
	bool flag = false;
	while (!_kbhit()) {
		cout << (flag ?  "YES\tno" : "yes\tNO") << '\r';
		flag ? flag = false : flag = true;
		this_thread::sleep_for(500ms);
	}
	if (flag)
		return 0;

	win = FindWindow(L"Сапер", NULL);	//get "Miner"-handle
	dc = GetDC(win); //get device context for the "Miner"-handle

	RECT win_rect{ 0 };
	GetWindowRect(win, &win_rect);
	pos.begin_on_field = { 13, 55 };
	pos.begin_on_screen = { win_rect.left + 15, win_rect.top + 101 };
	
	map.size.cx = (win_rect.right - pos.begin_on_screen.x) / step;
	map.size.cy = (win_rect.bottom - pos.begin_on_screen.y) / step;

	map.field = new char*[map.size.cy];
	for (short i = 0; i < map.size.cy; i++)
		map.field[i] = new char[map.size.cx];

	if (map.size.cx == 9)
		map.num_bombs = 10;
	else if (map.size.cx == 16)
		map.num_bombs = 40;
	else if (map.size.cx == 30)
		map.num_bombs = 99;
	else {
		short num;
		cout << "How many bombs do you use?" << endl;
		cin >> num;
		map.num_bombs = num;
	}

	beginGame();

	ReleaseDC(win, dc);
	system("pause");
	return 0;
}