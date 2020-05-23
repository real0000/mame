// license:GPL-2.0+
// copyright-holders:Couriersud

///
/// \file nl_factory.h
///

#ifndef NLFACTORY_H_
#define NLFACTORY_H_

#include "nltypes.h"
#include "plib/palloc.h"
#include "plib/ptypes.h"
#include "plib/putil.h"

#include <vector>

#define NETLIB_DEVICE_IMPL_ALIAS(p_alias, chip, p_name, p_def_param) \
	NETLIB_DEVICE_IMPL_BASE(devices, p_alias, chip, p_name, p_def_param) \

#define NETLIB_DEVICE_IMPL(chip, p_name, p_def_param) \
	NETLIB_DEVICE_IMPL_NS(devices, chip, p_name, p_def_param)

#define NETLIB_DEVICE_IMPL_NS(ns, chip, p_name, p_def_param) \
	NETLIB_DEVICE_IMPL_BASE(ns, chip, chip, p_name, p_def_param) \

#define NETLIB_DEVICE_IMPL_BASE(ns, p_alias, chip, p_name, p_def_param) \
	static plib::unique_ptr<factory::element_t> NETLIB_NAME(p_alias ## _c) \
			() \
	{ \
		using devtype = factory::device_element_t<ns :: NETLIB_NAME(chip)>; \
		auto sl(factory::properties(p_def_param, PSOURCELOC())); \
		return devtype::create(p_name, std::move(sl)); \
	} \
	\
	factory::constructor_ptr_t decl_ ## p_alias = NETLIB_NAME(p_alias ## _c);

namespace netlist {
	class core_device_t;
	class nlparse_t;
	class setup_t;
	class netlist_state_t;

namespace factory {

	struct properties
	{
		properties(const pstring &defparam, plib::source_location &&sourceloc)
		: m_defparam(defparam)
		, m_sourceloc(std::move(sourceloc))
		{ }

		PCOPYASSIGNMOVE(properties, default)

		const pstring &defparam() const noexcept
		{
			return m_defparam;
		}

		const plib::source_location &source() const noexcept
		{
			return m_sourceloc;
		}
	private:
		pstring m_defparam;
		plib::source_location m_sourceloc;
	};

	// -----------------------------------------------------------------------------
	// net_dev class factory
	// -----------------------------------------------------------------------------

	class element_t
	{
	public:
		element_t(const pstring &name, properties &&props);
		virtual ~element_t() = default;

		PCOPYASSIGNMOVE(element_t, default)

		virtual unique_pool_ptr<core_device_t> make_device(nlmempool &pool,
			netlist_state_t &anetlist,
			const pstring &name) = 0;

		virtual void macro_actions(nlparse_t &nparser, const pstring &name)
		{
			plib::unused_var(nparser);
			plib::unused_var(name);
		}

		const pstring &name() const noexcept { return m_name; }
		const pstring &param_desc() const noexcept { return m_properties.defparam(); }
		const plib::source_location &source() const noexcept { return m_properties.source(); }

	private:
		pstring m_name;                             ///< device name
		properties m_properties;                    ///< source file and other information and settings
	};

	template <class C>
	class device_element_t : public element_t
	{
	public:

		using device_ptr = unique_pool_ptr<core_device_t>;
		using element_ptr = plib::unique_ptr<device_element_t<C>>;

		device_element_t(const pstring &name, properties &&props)
		: element_t(name, std::move(props)) { }

		device_ptr make_device(nlmempool &pool,
			netlist_state_t &anetlist,
			const pstring &name) override
		{
			return pool.make_unique<C>(anetlist, name);
		}

		static element_ptr create(const pstring &name, properties &&props)
		{
			return plib::make_unique<device_element_t<C>>(name, std::move(props));
		}
	};

	class list_t : public std::vector<plib::unique_ptr<element_t>>
	{
	public:
		explicit list_t(log_type &alog);
		~list_t() = default;

		PCOPYASSIGNMOVE(list_t, delete)

		template<class device_class>
		void add(const pstring &name, properties &&props)
		{
			add(device_element_t<device_class>::create(name, std::move(props)));
		}

		void add(plib::unique_ptr<element_t> &&factory) noexcept(false);

		element_t * factory_by_name(const pstring &devname) noexcept(false);

		template <class C>
		bool is_class(element_t *f) noexcept
		{
			return dynamic_cast<device_element_t<C> *>(f) != nullptr;
		}

	private:
		log_type &m_log;
	};

	// -----------------------------------------------------------------------------
	// factory_creator_ptr_t
	// -----------------------------------------------------------------------------

	using constructor_ptr_t = plib::unique_ptr<element_t> (*)();

	template <typename T>
	plib::unique_ptr<element_t> constructor_t(const pstring &name, properties &&props)
	{
		return plib::make_unique<device_element_t<T>>(name, std::move(props));
	}

	// -----------------------------------------------------------------------------
	// factory_lib_entry_t: factory class to wrap macro based chips/elements
	// -----------------------------------------------------------------------------

	class library_element_t : public element_t
	{
	public:

		library_element_t(const pstring &name, properties &&props)
		: element_t(name, std::move(props))
		{
		}

		unique_pool_ptr<core_device_t> make_device(nlmempool &pool,
			netlist_state_t &anetlist,
			const pstring &name) override;

		void macro_actions(nlparse_t &nparser, const pstring &name) override;

	private:
	};

	} // namespace factory

	namespace devices {
		void initialize_factory(factory::list_t &factory);
	} // namespace devices
} // namespace netlist

#endif // NLFACTORY_H_
