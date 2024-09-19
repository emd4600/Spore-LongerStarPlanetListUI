// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"

void Initialize()
{
	// This method is executed when the game starts, before the user interface is shown
	// Here you can do things such as:
	//  - Add new cheats
	//  - Add new simulator classes
	//  - Add new game modes
	//  - Add new space tools
	//  - Change materials
}

void Dispose()
{
	// This method is called when the game is closing
}

using namespace Simulator;

member_detour(SpaceGameUI_FillStarTooltipPlanetInfo__detour, UI::SpaceGameUI,
	bool(UTFWin::UILayout*, int, cPlanetRecord*))
{
	bool detoured(UTFWin::UILayout* layout, int slotIndex, cPlanetRecord* planetRecord)
	{
		// Spore only has five slots; this function returns immediately when slotIndex >= 5 in vanilla game
		const int maxVanillaPlanets = 5;
		const uint32_t baseId = 0x0684BC00;
		// Spore hides only 5 slots in the caller of this function, but it is too big to detour
		// So instead, we hide all our extra slots when generating the first one
		if (slotIndex == 0) {
			auto parentWindow = layout->FindWindowByID(0x0684BC40);
			for (auto window : parentWindow->children()) {
				auto controlId = window->GetControlID();
				// Hide only slots: windows with control id multiple of 0x10
				int index = (controlId - baseId) / 0x10;
				if ((controlId & 0xF) == 0 && index >= maxVanillaPlanets && index <= 50) {
					window->SetVisible(false);
				}
			}
		}
		// Call original function for first 5 slots
		if (slotIndex < maxVanillaPlanets) {
			return original_function(this, layout, slotIndex, planetRecord);
		}
		// Ignore destroyed and gas planets
		if (planetRecord->IsDestroyed()) {
			return false;
		}
		if (planetRecord->mType < PlanetType::T0 || planetRecord->mType > PlanetType::T3) {
			return false;
		}

		uint32_t slotControlId = baseId + slotIndex * 0x10;
		IWindowPtr slotWindow = layout->FindWindowByID(slotControlId);
		if (!slotWindow) {
			// Duplicate the previous slot window and move it below
			uint32_t previousSlotControlId = baseId + (slotIndex - 1) * 0x10;
			auto previousSlotWindow = layout->FindWindowByID(previousSlotControlId);
			slotWindow = new UTFWin::Window();
			slotWindow->SetControlID(slotControlId);

			Math::Rectangle area = previousSlotWindow->GetArea();
			auto height = area.GetHeight();
			area.y1 += height;
			area.y2 += height;
			slotWindow->SetArea(area);

			auto layoutAnchor = UTFWin::kAnchorLeft | UTFWin::kAnchorRight | UTFWin::kAnchorTop;
			slotWindow->AddWinProc(new UTFWin::SimpleLayout(layoutAnchor));

			slotWindow->SetShadeColor(previousSlotWindow->GetShadeColor());
			slotWindow->SetFillColor(previousSlotWindow->GetFillColor());
			slotWindow->SetDrawable(previousSlotWindow->GetDrawable());
			slotWindow->SetTextFontID(previousSlotWindow->GetTextFontID());

			// Copy small, medium, large planet, species, tech level, spice icons and planet name text
			for (int i = 1; i <= 7; i++) {
				auto originalWindow = layout->FindWindowByID(previousSlotControlId + i);

				IWindowPtr window;
				// 6 is the planet name text, it is Outlined Text (0x039a721c) class
				if (i == 6) {
					auto outlinedText = ClassManager.Create(0x039a721c, UTFWin::GetAllocator());
					window = object_cast<UTFWin::IWindow>(outlinedText);
				} else {
					window = new UTFWin::Window();
				}

				window->SetControlID(slotControlId + i);
				window->SetArea(originalWindow->GetArea());
				window->SetFlag(UTFWin::kWinFlagIgnoreMouse, true);
				window->SetDrawable(originalWindow->GetDrawable());
				window->SetFillColor(originalWindow->GetFillColor());
				window->SetShadeColor(originalWindow->GetShadeColor());
				window->SetTextFontID(originalWindow->GetTextFontID());

				if (i == 7) {
					window->AddWinProc(new UTFWin::SimpleLayout(UTFWin::kAnchorRight | UTFWin::kAnchorTop));
				}

				slotWindow->AddWindow(window.get());
			}

			// Add to parent window
			auto parentWindow = previousSlotWindow->GetParent();
			parentWindow->AddWindow(slotWindow.get());
		}

		/* -- This is the same code as Spore -- */

		int planetSize = 1;
		if (planetRecord->IsMoon()) {
			planetSize = 2;
		}
		Math::Color planetShadeColor;
		auto tScore = TerraformingManager.GetTScore(planetRecord);
		if (tScore > 0) {
			planetShadeColor = 0xFF7DD244;
		}
		else if (planetRecord->mTemperatureScore > 0.75f) {
			planetShadeColor = 0xFFD42727;
		}
		else if (planetRecord->mTemperatureScore > 0.25f) {
			planetShadeColor = 0xFF21A5E7;
		}
		else {
			planetShadeColor = 0xFFFFFFFF;
		}

		slotWindow->SetVisible(true);

		auto smallPlanetWindow = layout->FindWindowByID(slotControlId + 1);
		auto mediumPlanetWindow = layout->FindWindowByID(slotControlId + 2);
		auto largePlanetWindow = layout->FindWindowByID(slotControlId + 3);
		auto spiceIconWindow = layout->FindWindowByID(slotControlId + 7);
		smallPlanetWindow->SetVisible(false);
		mediumPlanetWindow->SetVisible(false);
		largePlanetWindow->SetVisible(false);
		spiceIconWindow->SetVisible(false);

		UTFWin::IWindow* planetSizeWindow;
		if (planetSize == 0) {
			planetSizeWindow = largePlanetWindow;
		} else if (planetSize == 1) {
			planetSizeWindow = mediumPlanetWindow;
		} else {
			planetSizeWindow = smallPlanetWindow;
		}
		planetSizeWindow->SetVisible(true);
		planetSizeWindow->SetShadeColor(planetShadeColor);

		bool hasTechLevel = planetRecord->mTechLevel > TechLevel::None;
		auto techLevelWindow = layout->FindWindowByID(slotControlId + 5);
		techLevelWindow->SetVisible(hasTechLevel);

		if (planetRecord == GetPlayerHomePlanet()) {
			UTFWin::IImageDrawable::SetImageForWindow(techLevelWindow, { 0x32457B1, TypeIDs::png, GroupIDs::GameEntryImages });
		}
		else if (cPlanetRecord::HasControlledCity(planetRecord, GetPlayerEmpire())) {
			UTFWin::IImageDrawable::SetImageForWindow(techLevelWindow, { id("cip_colonyicon"), TypeIDs::png, GroupIDs::EditorPlannerImages});
		}
		else {
			if (hasTechLevel) {
				UTFWin::IImageDrawable::SetImageForWindow(techLevelWindow, cPlanetRecord::GetTypeIconKey(planetRecord));
			}
		}

		auto speciesProfile = (planetRecord->mTechLevel > TechLevel::Creature) ? 
			SpeciesManager.GetSpeciesProfile(planetRecord->GetCitizenSpeciesKey()) : nullptr;

		auto speciesIconWindow = layout->FindWindowByID(slotControlId + 4);
		speciesIconWindow->SetVisible(speciesProfile != nullptr);
		if (speciesProfile) {
			UTFWin::IImageDrawable::SetImageForWindow(speciesIconWindow, Simulator::GetMainSpeciesImageKey(planetRecord));
		}

		auto textWindow = layout->FindWindowByID(slotControlId + 6);
		textWindow->SetVisible(true);
		textWindow->SetCaption(planetRecord->mName.c_str());

		if (hasTechLevel) {
			auto terrainScriptSource = planetRecord->GetTerrainScriptSource().instanceID;
			if (terrainScriptSource != id("DestroyedAlienShip") && terrainScriptSource != id("DestroyedAlienCity01")) {
				PropertyListPtr propList;
				PropManager.GetPropertyList(planetRecord->mSpiceGen.instanceID, GroupIDs::SpaceTrading_, propList);

				uint32_t spaceEconomySpiceColor;
				if (App::Property::GetUInt32(propList.get(), 0x58CBB75, spaceEconomySpiceColor)) {
					spiceIconWindow->SetShadeColor(0xFF000000 | spaceEconomySpiceColor);
					spiceIconWindow->SetVisible(true);
				}
			}
		}

		return true;
	}
};


void AttachDetours()
{
	SpaceGameUI_FillStarTooltipPlanetInfo__detour::attach(GetAddress(UI::SpaceGameUI, FillStarTooltipPlanetInfo));
}


// Generally, you don't need to touch any code here
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		ModAPI::AddPostInitFunction(Initialize);
		ModAPI::AddDisposeFunction(Dispose);

		PrepareDetours(hModule);
		AttachDetours();
		CommitDetours();
		break;

	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

