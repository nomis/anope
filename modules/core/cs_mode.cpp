/* ChanServ core functions
 *
 * (C) 2003-2011 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "module.h"

class CommandCSMode : public Command
{
	void DoLock(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;
		ChannelInfo *ci = source.ci;
		const Anope::string &subcommand = params[2];
		const Anope::string &param = params.size() > 3 ? params[3] : "";

		if (subcommand.equals_ci("ADD") && !param.empty())
		{
			spacesepstream sep(param);
			Anope::string modes;

			sep.GetToken(modes);
			
			int adding = -1;
			for (size_t i = 0; i < modes.length(); ++i)
			{
				switch (modes[i])
				{
					case '+':
						adding = 1;
						break;
					case '-':
						adding = 0;
						break;
					default:
						if (adding == -1)
							break;
						ChannelMode *cm = ModeManager::FindChannelModeByChar(modes[i]);
						if (!cm || !cm->CanSet(u))
						{
							source.Reply(_("Unknown mode character %c ignored."), modes[i]);
							break;
						}
						Anope::string mode_param;
						if (((cm->Type == MODE_STATUS || cm->Type == MODE_LIST) && !sep.GetToken(mode_param)) || (cm->Type == MODE_PARAM && adding && !sep.GetToken(mode_param)))
							source.Reply(_("Missing parameter for mode %c."), cm->ModeChar);
						else
						{
							ci->SetMLock(cm, adding, mode_param, u->nick); 
							if (!mode_param.empty())
								mode_param = " " + mode_param;
							source.Reply(_("%c%c%s locked on %s"), adding ? '+' : '-', cm->ModeChar, mode_param.c_str(), ci->name.c_str());
						}
				}
			}

			if (ci->c)
				check_modes(ci->c);
		}
		else if (subcommand.equals_ci("DEL") && !param.empty())
		{
			spacesepstream sep(param);
			Anope::string modes;

			sep.GetToken(modes);

			int adding = -1;
			for (size_t i = 0; i < modes.length(); ++i)
			{
				switch (modes[i])
				{
					case '+':
						adding = 1;
						break;
					case '-':
						adding = 0;
						break;
					default:
						if (adding == -1)
							break;
						ChannelMode *cm = ModeManager::FindChannelModeByChar(modes[i]);
						if (!cm || !cm->CanSet(u))
						{
							source.Reply(_("Unknown mode character %c ignored."), modes[i]);
							break;
						}
						Anope::string mode_param;
						if (!cm->Type == MODE_REGULAR && !sep.GetToken(mode_param))
							source.Reply(_("Missing parameter for mode %c."), cm->ModeChar);
						else
						{
							if (ci->RemoveMLock(cm, mode_param))
							{
								if (!mode_param.empty())
									mode_param = " " + mode_param;
								source.Reply(_("%c%c%s has been unlocked from %s."), adding == 1 ? '+' : '-', cm->ModeChar, mode_param.c_str(), ci->name.c_str());
							}
							else
								source.Reply(_("%c is not locked on %s."), cm->ModeChar, ci->name.c_str());
						}
				}
			}
		}
		else if (subcommand.equals_ci("LIST"))
		{
			const std::multimap<ChannelModeName, ModeLock> &mlocks = ci->GetMLock();
			if (mlocks.empty())
			{
				source.Reply(_("Channel %s has no mode locks."), ci->name.c_str());
			}
			else
			{
				source.Reply(_("Mode locks for %s:"), ci->name.c_str());
				for (std::multimap<ChannelModeName, ModeLock>::const_iterator it = mlocks.begin(), it_end = mlocks.end(); it != it_end; ++it)
				{
					const ModeLock &ml = it->second;
					ChannelMode *cm = ModeManager::FindChannelModeByName(ml.name);
					if (!cm)
						continue;

					Anope::string modeparam = ml.param;
					if (!modeparam.empty())
						modeparam = " " + modeparam;
					Anope::string setter = ml.setter;
					if (setter.empty())
						setter = ci->founder ? ci->founder->display : "Unknown";
					source.Reply(_("%c%c%s, by %s on %s"), ml.set ? '+' : '-', cm->ModeChar, modeparam.c_str(), setter.c_str(), do_strftime(ml.created).c_str());
				}
			}
		}
		else
			this->OnSyntaxError(source, subcommand);
	}
	
	void DoSet(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;
		ChannelInfo *ci = source.ci;

		spacesepstream sep(params.size() > 3 ? params[3] : "");
		Anope::string modes = params[2], param;

		Log(LOG_COMMAND, u, this, ci) << "to set " << params[2];

		int adding = -1;
		for (size_t i = 0; i < modes.length(); ++i)
		{
			switch (modes[i])
			{
				case '+':
					adding = 1;
					break;
				case '-':
					adding = 0;
					break;
				case '*':
					if (adding == -1)
						break;
					for (std::map<Anope::string, Mode *>::const_iterator it = ModeManager::Modes.begin(), it_end = ModeManager::Modes.end(); it != it_end; ++it)
					{
						Mode *m = it->second;
						if (m->Class == MC_CHANNEL)
						{
							ChannelMode *cm = debug_cast<ChannelMode *>(m);
							if (cm->Type == MODE_REGULAR || (!adding && cm->Type == MODE_PARAM))
							{
								if (!cm->CanSet(u))
									continue;
								if (adding)
									ci->c->SetMode(NULL, cm);
								else
									ci->c->RemoveMode(NULL, cm);
							}
						}
					}
					break;
				default:
					if (adding == -1)
						break;
					ChannelMode *cm = ModeManager::FindChannelModeByChar(modes[i]);
					if (!cm || !cm->CanSet(u))
						continue;
					switch (cm->Type)
					{
						case MODE_REGULAR:
							if (adding)
								ci->c->SetMode(NULL, cm);
							else
								ci->c->RemoveMode(NULL, cm);
							break;
						case MODE_PARAM:
							if (adding && !sep.GetToken(param))
								break;
							if (adding)
								ci->c->SetMode(NULL, cm, param);
							else
								ci->c->RemoveMode(NULL, cm);
							break;
						case MODE_STATUS:
							if (!sep.GetToken(param))
								break;
							if (str_is_wildcard(param))
							{
								for (CUserList::const_iterator it = ci->c->users.begin(), it_end = ci->c->users.end(); it != it_end; ++it)
								{
									UserContainer *uc = *it;

									if (Anope::Match(u->GetMask(), param))
									{
										if (adding)
											ci->c->SetMode(NULL, cm, uc->user->nick);
										else
											ci->c->RemoveMode(NULL, cm, uc->user->nick);
									}
								}
							}
							else
							{
								if (adding)
									ci->c->SetMode(NULL, cm, param);
								else
									ci->c->RemoveMode(NULL, cm, param);
							}
							break;
						case MODE_LIST:
							if (!sep.GetToken(param))
								break;
							if (adding)
								ci->c->SetMode(NULL, cm, param);
							else
							{
								std::pair<Channel::ModeList::iterator, Channel::ModeList::iterator> its = ci->c->GetModeList(cm->Name);
								for (; its.first != its.second;)
								{
									const Anope::string &mask = its.first->second;
									++its.first;

									if (Anope::Match(mask, param))
										ci->c->RemoveMode(NULL, cm, mask);
								}
							}
					}
			}
		}
	}

 public:
	CommandCSMode() : Command("MODE", 3, 4)
	{
		this->SetDesc("Control modes and mode locks on a channel");
	}

	CommandReturn Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		const Anope::string &subcommand = params[1];

		User *u = source.u;
		ChannelInfo *ci = source.ci;

		if (!ci || !ci->c)
			source.Reply(LanguageString::CHAN_X_NOT_IN_USE, ci->name.c_str());
		else if (!check_access(u, ci, CA_MODE) && !u->Account()->HasCommand("chanserv/mode"))
			source.Reply(LanguageString::ACCESS_DENIED);
		else if (subcommand.equals_ci("LOCK"))
			this->DoLock(source, params);
		else if (subcommand.equals_ci("SET"))
			this->DoSet(source, params);
		else
			this->OnSyntaxError(source, "");

		return MOD_CONT;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		source.Reply(_("Syntax: \002MODE \037channel\037 LOCK {ADD|DEL|LIST} [\037what\037]\002\n"
			"        \002MODE \037channel\037 SET \037modes\037\002\n"
			" \n"
			"Mainly controls mode locks and mode access (which is different from channel access)\n"
			"on a channel.\n"
			" \n"
			"The \002MODE LOCK\002 command allows you to add, delete, and view mode locks on a channel.\n"
			"If a mode is locked on or off, services will not allow that mode to be changed.\n"
			"Example:\n"
			"     \002MODE #channel LOCK ADD +bmnt *!*@*aol*\002\n"
			" \n"
			"The \002MODE SET\002 command allows you to set modes through services. Wildcards * and ? may\n"
			"be given as parameters for list and status modes.\n"
			"Example:\n"
			"     \002MODE #channel SET +v *\002\n"
			"       Sets voice status to all users in the channel.\n"
			" \n"
			"     \002MODE #channel SET -b ~c:*\n"
			"       Clears all extended bans that start with ~c:"));
		return true;
	}

	void OnSyntaxError(CommandSource &source, const Anope::string &subcommand)
	{
		SyntaxError(source, "MODE", _("MODE \037channel\037 {LOCK|SET} [\037modes\037 | {ADD|DEL|LIST} [\037what\037]]"));
	}
};

class CSMode : public Module
{
	CommandCSMode commandcsmode;

 public:
	CSMode(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetType(CORE);

		this->AddCommand(ChanServ, &commandcsmode);
	}
};

MODULE_INIT(CSMode)