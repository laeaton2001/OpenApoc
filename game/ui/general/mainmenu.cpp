#include "game/ui/general/mainmenu.h"
#include "forms/ui.h"
#include "framework/event.h"
#include "framework/framework.h"
#include "game/ui/city/cityview.h"
#include "game/ui/debugtools/debugmenu.h"
#include "game/ui/general/difficultymenu.h"
#include "game/ui/general/loadingscreen.h"
#include "game/ui/general/optionsmenu.h"
#include "game/ui/general/savemenu.h"
#include "version.h"

namespace OpenApoc
{

static std::vector<UString> tracks{"music:0", "music:1", "music:2"};

MainMenu::MainMenu() : Stage(), mainmenuform(ui().GetForm("FORM_MAINMENU"))
{
	auto versionLabel = mainmenuform->FindControlTyped<Label>("VERSION_LABEL");
	versionLabel->SetText(OPENAPOC_VERSION);
}

MainMenu::~MainMenu() = default;

void MainMenu::Begin() { fw().jukebox->play(tracks); }

void MainMenu::Pause() {}

void MainMenu::Resume() {}

void MainMenu::Finish() {}

void MainMenu::EventOccurred(Event *e)
{
	mainmenuform->EventOccured(e);

	if (e->Type() == EVENT_KEY_DOWN)
	{
		if (e->Keyboard().KeyCode == SDLK_ESCAPE)
		{
			stageCmd.cmd = StageCmd::Command::QUIT;
			return;
		}
	}

	if (e->Type() == EVENT_FORM_INTERACTION && e->Forms().EventFlag == FormEventType::ButtonClick)
	{
		if (e->Forms().RaisedBy->Name == "BUTTON_OPTIONS")
		{
			stageCmd.cmd = StageCmd::Command::PUSH;
			stageCmd.nextStage = mksp<OptionsMenu>();
			return;
		}
		if (e->Forms().RaisedBy->Name == "BUTTON_QUIT")
		{
			stageCmd.cmd = StageCmd::Command::QUIT;
			return;
		}
		if (e->Forms().RaisedBy->Name == "BUTTON_NEWGAME")
		{
			stageCmd.cmd = StageCmd::Command::PUSH;
			stageCmd.nextStage = mksp<DifficultyMenu>();
			return;
		}
		if (e->Forms().RaisedBy->Name == "BUTTON_DEBUG")
		{
			stageCmd.cmd = StageCmd::Command::PUSH;
			stageCmd.nextStage = mksp<DebugMenu>();
			return;
		}
		if (e->Forms().RaisedBy->Name == "BUTTON_LOADGAME")
		{
			stageCmd.cmd = StageCmd::Command::PUSH;
			stageCmd.nextStage = mksp<SaveMenu>(SaveMenuAction::LoadNewGame, nullptr);
			return;
		}
	}
}

void MainMenu::Update(StageCmd *const cmd)
{
	mainmenuform->Update();
	*cmd = stageCmd;
	stageCmd = StageCmd();
}

void MainMenu::Render() { mainmenuform->Render(); }

bool MainMenu::IsTransition() { return false; }
}; // namespace OpenApoc
