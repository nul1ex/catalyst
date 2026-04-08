#pragma once

class memory
{
public:
	bool initialize( std::wstring_view process_name );
	bool read( std::uintptr_t address, void* buffer, std::size_t size ) const;

	template <typename T>
	T read( std::uintptr_t address ) const
	{
		T value{};
		this->read( address, &value, sizeof( T ) );
		return value;
	}

	bool write( std::uintptr_t address, const void* buffer, std::size_t size ) const;

	template <typename T>
	bool write( std::uintptr_t address, const T& value ) const
	{
		return this->write( address, &value, sizeof( T ) );
	}


	HANDLE create_thread(uintptr_t funcAddress, LPVOID param = nullptr) const
	{
		if (!this->handle() || !funcAddress)
			return nullptr;

		HANDLE hThread = CreateRemoteThread(
			this->handle(),
			nullptr,              // default security
			0,                    // default stack
			reinterpret_cast<LPTHREAD_START_ROUTINE>(funcAddress),
			param,                // optional parameter
			0,                    // run immediately
			nullptr               // thread ID
		);

		return hThread; // caller should CloseHandle
	}

	void call_remote(uintptr_t funcAddress, LPVOID arg = nullptr)
	{
		if (!funcAddress)
			return;

		HANDLE hThread = create_thread(funcAddress, arg);
		if (hThread) {
			WaitForSingleObject(hThread, INFINITE);
			CloseHandle(hThread);
		}
	}

	template <typename T = std::uintptr_t>
	T resolve_rip( std::uintptr_t address, std::int32_t offset = 3, std::int32_t length = 7 ) const
	{
		const auto rva = this->read<std::int32_t>( address + offset );
		const auto resolved = address + length + rva;

		if constexpr ( std::is_pointer_v<T> )
		{
			return reinterpret_cast< T >( resolved );
		}
		else
		{
			return static_cast< T >( resolved );
		}
	}

	template <typename T = std::uintptr_t>
	T resolve_call( std::uintptr_t address ) const
	{
		return this->resolve_rip<T>( address, 1, 5 );
	}

	std::string read_string( std::uintptr_t address, std::size_t max_len = 256 ) const;
	std::uintptr_t find_pattern( std::uintptr_t module_base, std::string_view pattern ) const;
	std::uintptr_t find_vtable( std::uintptr_t module_base, std::string_view class_name ) const;
	std::uintptr_t find_vtable_instance( std::uintptr_t module_base, std::string_view class_name ) const;
	std::uintptr_t get_module( std::string_view name ) const;
	std::uintptr_t allocate( std::uintptr_t address = 0, std::size_t size = 0x1000, std::uint32_t protection = PAGE_EXECUTE_READWRITE ) const;
	bool free( std::uintptr_t address ) const;
	std::uintptr_t get_base() const { return this->m_base; }

	[[nodiscard]] void* handle( ) const { return this->m_handle; }
	[[nodiscard]] std::uint32_t process_id( ) const { return this->m_id; }

private:
	std::uint32_t m_id{};
	void* m_handle{};
	std::uintptr_t m_base{};

	std::size_t get_module_size( std::uintptr_t module_base ) const;
	std::uintptr_t find_qword_in_sections( std::uintptr_t module_base, std::uintptr_t value, std::uint32_t section_filter ) const;
};