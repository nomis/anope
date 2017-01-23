/*
 * Anope IRC Services
 *
 * Copyright (C) 2010-2016 Anope Team <team@anope.org>
 *
 * This file is part of Anope. Anope is free software; you can
 * redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see see <http://www.gnu.org/licenses/>.
 */

#include "module.h"
#include "modules/nickserv/set_misc.h"
#include "modules/nickserv/info.h"
#include "modules/nickserv/set.h"

static Anope::map<Anope::string> descriptions;

class NSMiscDataImpl : public NSMiscData
{
	friend class NSMiscDataType;

	Serialize::Storage<NickServ::Account *> account;
	Serialize::Storage<Anope::string> name, data;

 public:
	using NSMiscData::NSMiscData;

	NickServ::Account *GetAccount() override;
	void SetAccount(NickServ::Account *s) override;

	Anope::string GetName() override;
	void SetName(const Anope::string &n) override;

	Anope::string GetData() override;
	void SetData(const Anope::string &d) override;
};

class NSMiscDataType : public Serialize::Type<NSMiscDataImpl>
{
 public:
	Serialize::ObjectField<NSMiscDataImpl, NickServ::Account *> owner;
	Serialize::Field<NSMiscDataImpl, Anope::string> name, data;

	NSMiscDataType(Module *me) : Serialize::Type<NSMiscDataImpl>(me)
		, owner(this, "account", &NSMiscDataImpl::account, true)
		, name(this, "name", &NSMiscDataImpl::name)
		, data(this, "data", &NSMiscDataImpl::data)
	{
	}
};

NickServ::Account *NSMiscDataImpl::GetAccount()
{
	return Get(&NSMiscDataType::owner);
}

void NSMiscDataImpl::SetAccount(NickServ::Account *s)
{
	Set(&NSMiscDataType::owner, s);
}

Anope::string NSMiscDataImpl::GetName()
{
	return Get(&NSMiscDataType::name);
}

void NSMiscDataImpl::SetName(const Anope::string &n)
{
	Set(&NSMiscDataType::name, n);
}

Anope::string NSMiscDataImpl::GetData()
{
	return Get(&NSMiscDataType::data);
}

void NSMiscDataImpl::SetData(const Anope::string &d)
{
	Set(&NSMiscDataType::data, d);
}

class CommandNSSetMisc : public Command
{
	Anope::string GetAttribute(const Anope::string &command)
	{
		size_t sp = command.rfind(' ');
		if (sp != Anope::string::npos)
			return command.substr(sp + 1);
		return command;
	}

 public:
	CommandNSSetMisc(Module *creator, const Anope::string &cname = "nickserv/set/misc", size_t min = 0) : Command(creator, cname, min, min + 1)
	{
		this->SetSyntax(_("[\037parameter\037]"));
	}

	void Run(CommandSource &source, const Anope::string &user, const Anope::string &param)
	{
		if (Anope::ReadOnly)
		{
			source.Reply(_("Services are in read-only mode."));
			return;
		}

		NickServ::Nick *na = NickServ::FindNick(user);
		if (!na)
		{
			source.Reply(_("\002{0}\002 isn't registered."), user);
			return;
		}
		NickServ::Account *nc = na->GetAccount();

		EventReturn MOD_RESULT = EventManager::Get()->Dispatch(&Event::SetNickOption::OnSetNickOption, source, this, nc, param);
		if (MOD_RESULT == EVENT_STOP)
			return;

		Anope::string scommand = GetAttribute(source.command);

		/* remove existing */
		for (NSMiscData *data : nc->GetRefs<NSMiscData *>())
			if (data->GetName() == scommand)
			{
				data->Delete();
				break;
			}

		if (!param.empty())
		{
			NSMiscData *data = Serialize::New<NSMiscData *>();
			data->SetAccount(nc);
			data->SetName(scommand);
			data->SetData(param);

			source.Reply(_("\002{0}\002 for \002{1}\002 set to \002{2}\002."), scommand, nc->GetDisplay(), param);
		}
		else
		{
			source.Reply(_("\002{0}\002 for \002{1}\002 unset."), scommand, nc->GetDisplay());
		}
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) override
	{
		this->Run(source, source.nc->GetDisplay(), !params.empty() ? params[0] : "");
	}

	void OnServHelp(CommandSource &source) override
	{
		if (descriptions.count(source.command))
		{
			this->SetDesc(descriptions[source.command]);
			Command::OnServHelp(source);
		}
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) override
	{
		if (descriptions.count(source.command))
		{
			source.Reply(Language::Translate(source.nc, descriptions[source.command].c_str()));
			return true;
		}
		return false;
	}
};

class CommandNSSASetMisc : public CommandNSSetMisc
{
 public:
	CommandNSSASetMisc(Module *creator) : CommandNSSetMisc(creator, "nickserv/saset/misc", 1)
	{
		this->ClearSyntax();
		this->SetSyntax(_("\037nickname\037 [\037parameter\037]"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) override
	{
		this->Run(source, params[0], params.size() > 1 ? params[1] : "");
	}
};

class NSSetMisc : public Module
	, public EventHook<Event::NickInfo>
{
	CommandNSSetMisc commandnssetmisc;
	CommandNSSASetMisc commandnssasetmisc;
	NSMiscDataType type;

 public:
	NSSetMisc(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, VENDOR)
		, EventHook<Event::NickInfo>(this)
		, commandnssetmisc(this)
		, commandnssasetmisc(this)
		, type(this)
	{
	}

	void OnReload(Configuration::Conf *conf) override
	{
		descriptions.clear();

		for (int i = 0; i < conf->CountBlock("command"); ++i)
		{
			Configuration::Block *block = conf->GetBlock("command", i);

			const Anope::string &cmd = block->Get<Anope::string>("command");

			if (cmd != "nickserv/set/misc" && cmd != "nickserv/saset/misc")
				continue;

			Anope::string cname = block->Get<Anope::string>("name");
			Anope::string desc = block->Get<Anope::string>("misc_description");

			if (cname.empty() || desc.empty())
				continue;

			descriptions[cname] = desc;
		}
	}

	void OnNickInfo(CommandSource &source, NickServ::Nick *na, InfoFormatter &info, bool) override
	{
		for (NSMiscData *data : na->GetAccount()->GetRefs<NSMiscData *>())
			info[data->GetName()] = data->GetData();
	}
};

MODULE_INIT(NSSetMisc)