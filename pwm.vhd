-- pwm.vhd

-- This file was auto-generated as a prototype implementation of a module
-- created in component editor.  It ties off all outputs to ground and
-- ignores all inputs.  It needs to be edited to make it do something
-- useful.
-- 
-- This file will not be automatically regenerated.  You should check it in
-- to your version control system if you want to keep it.

library IEEE;
use IEEE.std_logic_1164.all;
use IEEE.numeric_std.all;

entity pwm is
	port (
		reset_n   : in  std_logic                     := '0';             --        reset.reset_n
		pwm_out   : out std_logic;                                        --      conduit.pwm_out
		clock     : in  std_logic                     := '0';             --        clock.clk
		writedata : in  std_logic_vector(31 downto 0) := (others => '0'); -- avalon_slave.writedata
		write     : in  std_logic                     := '0';             --             .write
		readdata  : out std_logic_vector(31 downto 0);                    --             .readdata
		read      : in  std_logic                     := '0';             --             .read
		cs        : in  std_logic                     := '0';             --             .chipselect
		address   : in  std_logic_vector(1 downto 0)  := (others => '0')  --             .address
	);
end entity pwm;

architecture rtl of pwm is
	component pwm_module
		port (
			clk, reset_n : in std_logic;
			data_in : in std_logic_vector(31 downto 0);	-- duty cycle
			data_out : out std_logic_vector(31 downto 0);
			pwm_out : out std_logic
		);
	end component;
		
signal stagingReg : std_logic_vector(31 downto 0);
		
begin
	the_pwm_module : pwm_module
		port map (
			clk => clock,
			reset_n => reset_n,
			data_in => stagingReg, --signal
			data_out => readdata,
			pwm_out => pwm_out
		);
	
	enable : process(clock, reset_n)
	begin
	   if(reset_n = '0') then
			stagingReg <= (others => '0'); --signal
		elsif (clock'event and clock = '1') then
			if (cs = '1') then
				if (write = '1') then
					stagingReg <= writedata; --signal
				end if;
			end if;
		end if;
	end process enable;

end rtl; -- of pwm
