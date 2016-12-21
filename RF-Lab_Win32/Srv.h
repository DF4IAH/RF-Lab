#pragma once

class Srv
{
private:
	bool m_ready;

public:
	Srv();
	~Srv();

	void paint(HWND hWnd, LPPAINTSTRUCT ps, HDC hdc);
	bool isReady();


	// Startet den Visualisierungsserver an
	static void SrvStart();

	// H�lt den Visualisierungsserver an
	static void SrvStop();

	static void SrvPaint(HWND hWnd, LPPAINTSTRUCT ps, HDC hdc);
};

