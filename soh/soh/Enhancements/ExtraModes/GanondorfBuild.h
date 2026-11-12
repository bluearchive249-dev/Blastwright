// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
#pragma once
// Play as Ganondorf: placed pieces (stairs, ramps, platforms...), see GanondorfBuild.cpp. Include after z64.h.

int GdfBuild_KindCount();
const char* GdfBuild_KindName(int kind);
int GdfBuild_CountHere();
int GdfBuild_MaxHere();
bool GdfBuild_IsPlacing();
bool GdfBuild_Focus(Vec3f* pos);
bool GdfBuild_Begin(int kind);
bool GdfBuild_PlacingFrame(Input* input);
bool GdfBuild_RemoveNearest();
int GdfBuild_ClearHere();
void GdfBuild_Cancel();
