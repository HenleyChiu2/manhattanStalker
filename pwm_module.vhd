library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity pwm_module is
	port (
		clk, reset_n : in std_logic;
		--count_le, duty_le : in std_logic;
		--run : in std_logic;	-- what's this for?
		data_in : in std_logic_vector(31 downto 0);	-- duty cycle?
		data_out : out std_logic_vector(31 downto 0);	-- what's this for?
		pwm_out : out std_logic
	);
end entity;	

architecture architectures of pwm_module is

constant full_cycle : unsigned(31 downto 0) := x"001E8480";	-- 20ms full cycle / 10ns clock cycle
--constant full_cycle : unsigned(31 downto 0) := x"0000000A";
signal cycle_count : unsigned(31 downto 0) := x"00000000";

type t_pwm_fsm is (	ST_HIGH,
							ST_LOW );
signal st_present : t_pwm_fsm := ST_HIGH;
signal st_next : t_pwm_fsm := ST_HIGH;

begin
	p_state: process(clk, reset_n)
	begin
		if(reset_n = '0') then
			data_out <= (others => '0');
			st_present <= ST_HIGH;
			cycle_count <= (others => '0');
		elsif (clk'event and clk = '1') then
			st_present <= st_next;
			if (cycle_count < full_cycle) then
				cycle_count <= cycle_count+1;
			else
				cycle_count <= (others => '0');
			end if;
			data_out <= std_logic_vector(cycle_count);
		end if;
	end process p_state;
  
	p_transition: process (
								st_present,
								cycle_count)
	begin
		case st_present is
			when ST_HIGH =>
				if (cycle_count < unsigned(data_in)) then
					st_next <= ST_HIGH;
				else
					st_next <= ST_LOW;
				end if;
			when ST_LOW =>
				if (cycle_count < full_cycle) then
					st_next <= ST_LOW;
				else
					st_next <= ST_HIGH;
				end if;
		end case;
					
	end process p_transition;
	
	p_state_out: process(clk, reset_n)
	begin
		if (reset_n = '0') then
			pwm_out <= '0';
		elsif (clk'event and clk = '1') then
			case st_present is
				when ST_HIGH =>
					pwm_out <= '1';
				when ST_LOW =>
					pwm_out <= '0';	
			end case;
		end if;
	end process p_state_out;					
  
end architectures;