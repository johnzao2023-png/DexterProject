#pragma once
#include <windows.h>
#include <unordered_map>
#include <string>

namespace render {

	static std::unordered_map<std::string, HFONT> fontCache;
	static std::unordered_map<int, HPEN> penCache;
	static std::unordered_map<int, HBRUSH> brushCache;
	static HFONT hWeaponFont = nullptr;
	static bool  weaponFontLoaded = false;

	void InitWeaponFont(int size = 18)
	{
		int result = AddFontResourceExA("CS2GunIcons.ttf", FR_PRIVATE, 0);

		if (result > 0) {
			hWeaponFont = CreateFontA(
				size, 0, 0, 0,
				FW_NORMAL,
				FALSE, FALSE, FALSE,
				DEFAULT_CHARSET,
				OUT_TT_PRECIS,
				CLIP_DEFAULT_PRECIS,
				NONANTIALIASED_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE,
				"CS2 Gun Icons"
			);
			weaponFontLoaded = (hWeaponFont != nullptr);
		}
	}

	void RenderWeaponIconCentered(HDC hdc, int centerX, int y, const char* icon, COLORREF color)
	{
		if (!hWeaponFont) return;

		if (color == RGB(255, 255, 255))
			color = RGB(220, 220, 220);

		HFONT oldFont = (HFONT)SelectObject(hdc, hWeaponFont);
		SetBkMode(hdc, TRANSPARENT);

		SIZE textSize;
		GetTextExtentPoint32A(hdc, icon, (int)strlen(icon), &textSize);
		int x = centerX - textSize.cx / 2;

		SetTextColor(hdc, RGB(0, 0, 0));
		TextOutA(hdc, x + 2, y, icon, (int)strlen(icon));
		TextOutA(hdc, x - 2, y, icon, (int)strlen(icon));
		TextOutA(hdc, x, y + 2, icon, (int)strlen(icon));
		TextOutA(hdc, x, y - 2, icon, (int)strlen(icon));
		TextOutA(hdc, x + 1, y + 1, icon, (int)strlen(icon));
		TextOutA(hdc, x - 1, y - 1, icon, (int)strlen(icon));
		TextOutA(hdc, x + 1, y - 1, icon, (int)strlen(icon));
		TextOutA(hdc, x - 1, y + 1, icon, (int)strlen(icon));

		SetTextColor(hdc, color);
		TextOutA(hdc, x, y, icon, (int)strlen(icon));

		SelectObject(hdc, oldFont);
	}

	void CleanupCache()
	{
		for (auto& pair : fontCache) DeleteObject(pair.second);
		fontCache.clear();
		for (auto& pair : penCache) DeleteObject(pair.second);
		penCache.clear();
		for (auto& pair : brushCache) DeleteObject(pair.second);
		brushCache.clear();

		if (hWeaponFont) { DeleteObject(hWeaponFont); hWeaponFont = nullptr; }
		RemoveFontResourceExA("CS2GunIcons.ttf", FR_PRIVATE, 0);
	}

	HFONT GetFont(const char* name, int size, int weight = FW_NORMAL)
	{
		std::string key = std::string(name) + "_" + std::to_string(size);
		if (fontCache.find(key) == fontCache.end()) {
			fontCache[key] = CreateFontA(size, 0, 0, 0, weight, FALSE, FALSE, FALSE,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE, name);
		}
		return fontCache[key];
	}

	HPEN GetPen(COLORREF color, int thickness = 1)
	{
		int colorInt = (int)color;
		int key = colorInt + (thickness * 1000000);
		if (penCache.find(key) == penCache.end()) {
			penCache[key] = CreatePen(PS_SOLID, thickness, color);
		}
		return penCache[key];
	}

	HBRUSH GetBrush(COLORREF color)
	{
		int colorInt = (int)color;
		if (brushCache.find(colorInt) == brushCache.end()) {
			brushCache[colorInt] = CreateSolidBrush(color);
		}
		return brushCache[colorInt];
	}

	void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color, int thickness = 1)
	{
		HPEN hPen = GetPen(color, thickness);
		HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
		MoveToEx(hdc, x1, y1, NULL);
		LineTo(hdc, x2, y2);
		SelectObject(hdc, oldPen);
	}

	void DrawFilledBox(HDC hdc, float x, float y, float w, float h, COLORREF color)
	{
		HBRUSH hBrush = GetBrush(color);
		RECT rect = { (LONG)x, (LONG)y, (LONG)(x + w), (LONG)(y + h) };
		FillRect(hdc, &rect, hBrush);
	}

	void DrawCircle(HDC hdc, int centerX, int centerY, int radius, COLORREF color, int thickness = 1)
	{
		HPEN hPen = GetPen(color, thickness);
		HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
		HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
		HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);

		Ellipse(hdc, centerX - radius, centerY - radius, centerX + radius, centerY + radius);

		SelectObject(hdc, oldPen);
		SelectObject(hdc, oldBrush);
	}

	void DrawCornerBox(HDC hdc, int x, int y, int width, int height, COLORREF color, COLORREF outlineColor, int thickness)
	{
		int cornerSize = width / 4;

		HPEN hPen = GetPen(color, thickness);
		HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

		MoveToEx(hdc, x, y + cornerSize, NULL);
		LineTo(hdc, x, y);
		LineTo(hdc, x + cornerSize, y);

		MoveToEx(hdc, x + width - cornerSize, y, NULL);
		LineTo(hdc, x + width, y);
		LineTo(hdc, x + width, y + cornerSize);

		MoveToEx(hdc, x + width, y + height - cornerSize, NULL);
		LineTo(hdc, x + width, y + height);
		LineTo(hdc, x + width - cornerSize, y + height);

		MoveToEx(hdc, x + cornerSize, y + height, NULL);
		LineTo(hdc, x, y + height);
		LineTo(hdc, x, y + height - cornerSize);

		SelectObject(hdc, oldPen);
	}

	void DrawBarOutlined(HDC hdc, int x, int y, int width, int height, float percentage, COLORREF barColor, COLORREF bgColor, COLORREF outlineColor)
	{
		HBRUSH hBrush = GetBrush(bgColor);
		RECT bgRect = { x, y, x + width, y + height };
		FillRect(hdc, &bgRect, hBrush);

		int filledHeight = (int)(height * percentage / 100.f);
		if (filledHeight > 0) {
			HBRUSH hBrushBar = GetBrush(barColor);
			RECT barRect = { x, y + height - filledHeight, x + width, y + height };
			FillRect(hdc, &barRect, hBrushBar);
		}

		HPEN hPen = GetPen(outlineColor, 1);
		HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
		HBRUSH hBrushNull = (HBRUSH)GetStockObject(NULL_BRUSH);
		HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrushNull);

		Rectangle(hdc, x, y, x + width, y + height);

		SelectObject(hdc, oldPen);
		SelectObject(hdc, oldBrush);
	}

	void RenderTextOutlined(HDC hdc, int x, int y, const char* text, COLORREF color, int fontSize)
	{
		HFONT hFont = GetFont("Arial", fontSize, FW_BOLD);
		HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
		SetBkMode(hdc, TRANSPARENT);

		// ✅ RENDERIZA OUTLINE PRETO
		SetTextColor(hdc, RGB(0, 0, 0));
		TextOutA(hdc, x - 1, y - 1, text, (int)strlen(text));
		TextOutA(hdc, x + 1, y - 1, text, (int)strlen(text));
		TextOutA(hdc, x - 1, y + 1, text, (int)strlen(text));
		TextOutA(hdc, x + 1, y + 1, text, (int)strlen(text));
		TextOutA(hdc, x - 1, y, text, (int)strlen(text));
		TextOutA(hdc, x + 1, y, text, (int)strlen(text));
		TextOutA(hdc, x, y - 1, text, (int)strlen(text));
		TextOutA(hdc, x, y + 1, text, (int)strlen(text));

		// ✅ RENDERIZA TEXTO COM A COR ORIGINAL
		SetTextColor(hdc, color);
		TextOutA(hdc, x, y, text, (int)strlen(text));

		SelectObject(hdc, oldFont);
	}

	void RenderTextOutlinedCentered(HDC hdc, int centerX, int centerY, const char* text, COLORREF color, int fontSize)
	{
		HFONT hFont = GetFont("Arial", fontSize, FW_BOLD);
		HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
		SetBkMode(hdc, TRANSPARENT);

		SIZE textSize;
		GetTextExtentPoint32A(hdc, text, (int)strlen(text), &textSize);

		int x = centerX - textSize.cx / 2;
		int y = centerY - textSize.cy / 2;

		// ✅ RENDERIZA OUTLINE PRETO
		SetTextColor(hdc, RGB(0, 0, 0));
		TextOutA(hdc, x - 1, y - 1, text, (int)strlen(text));
		TextOutA(hdc, x + 1, y - 1, text, (int)strlen(text));
		TextOutA(hdc, x - 1, y + 1, text, (int)strlen(text));
		TextOutA(hdc, x + 1, y + 1, text, (int)strlen(text));
		TextOutA(hdc, x - 1, y, text, (int)strlen(text));
		TextOutA(hdc, x + 1, y, text, (int)strlen(text));
		TextOutA(hdc, x, y - 1, text, (int)strlen(text));
		TextOutA(hdc, x, y + 1, text, (int)strlen(text));

		// ✅ RENDERIZA TEXTO COM A COR ORIGINAL
		SetTextColor(hdc, color);
		TextOutA(hdc, x, y, text, (int)strlen(text));

		SelectObject(hdc, oldFont);
	}
}