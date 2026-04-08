#pragma once

class render
{
public:
	struct loaded_fonts
	{
		zdraw::font* mochi_12{};
		zdraw::font* pretzel_12{};
		zdraw::font* pretzel_24{};
		zdraw::font* pixel7_10{};
		zdraw::font* weapons_15{};
		zdraw::font* weapons_40{};
	};

	bool initialize( );

	[[nodiscard]] const loaded_fonts& fonts( ) const { return this->m_fonts; }
	[[nodiscard]] HWND hwnd( ) const { return this->m_hwnd; }
	[[nodiscard]] explicit operator bool( ) const { return this->m_hwnd != nullptr; }

private:
	void run( );
	void update_input_window( );
	bool setup_d3d( );
	bool register_window_class( );

	static LRESULT CALLBACK wnd_proc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );

	HWND m_hwnd{};
	HWND m_input_hwnd{};
	ATOM m_atom{};
	bool m_input_visible{ false };

	ID3D11Device* m_device{};
	ID3D11DeviceContext* m_context{};
	IDXGISwapChain* m_swap_chain{};
	ID3D11RenderTargetView* m_rtv{};

	loaded_fonts m_fonts{};

	static constexpr const wchar_t* k_class_name{ L"catalyst.overlay" };
};