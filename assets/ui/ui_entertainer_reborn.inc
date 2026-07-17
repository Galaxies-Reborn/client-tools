	<Page BackgroundOpacity='0.82' BackgroundTint='#16373A' Location='20,420' Name='EntertainerReborn' RStyleDefault='/Styles.New.pill_med.rs_default' Size='390,164' UserMovable='true' Visible='false'>
		<Data Name='CodeData' device='device' flourish1='flourish1' flourish2='flourish2' flourish3='flourish3' flourish4='flourish4' flourish5='flourish5' flourish6='flourish6' flourish7='flourish7' flourish8='flourish8' mode='mode' octave='octave' pause='pause' settings='settings' song='song' stop='stop'/>
		<Text Font='bold_13' LocalText='MIDI PERFORMANCE' Location='12,8' Name='mode' PalText='header' Size='240,19'>MIDI PERFORMANCE</Text>
		<Text Font='bold_12' LocalText='' Location='12,31' Name='song' Size='365,19'></Text>
		<Text Font='bold_12' LocalText='' Location='12,52' Name='device' Size='365,19'></Text>
		<Text Font='bold_12' LocalText='Octave +0' Location='294,8' Name='octave' Size='84,19'>Octave +0</Text>
		<Button LocalText='1' Location='12,78' Name='flourish1' Size='36,24' Style='/Styles.New.buttons.hud.style'>1</Button><Button LocalText='2' Location='52,78' Name='flourish2' Size='36,24' Style='/Styles.New.buttons.hud.style'>2</Button><Button LocalText='3' Location='92,78' Name='flourish3' Size='36,24' Style='/Styles.New.buttons.hud.style'>3</Button><Button LocalText='4' Location='132,78' Name='flourish4' Size='36,24' Style='/Styles.New.buttons.hud.style'>4</Button>
		<Button LocalText='5' Location='172,78' Name='flourish5' Size='36,24' Style='/Styles.New.buttons.hud.style'>5</Button><Button LocalText='6' Location='212,78' Name='flourish6' Size='36,24' Style='/Styles.New.buttons.hud.style'>6</Button><Button LocalText='7' Location='252,78' Name='flourish7' Size='36,24' Style='/Styles.New.buttons.hud.style'>7</Button><Button LocalText='8' Location='292,78' Name='flourish8' Size='36,24' Style='/Styles.New.buttons.hud.style'>8</Button>
		<Button LocalText='Pause' Location='12,126' Name='pause' Size='96,22' Style='/Styles.New.buttons.hud.style'>Pause</Button><Button LocalText='Input Settings' Location='146,126' Name='settings' Size='110,22' Style='/Styles.New.buttons.hud.style'>Input Settings</Button><Button LocalText='Stop' Location='282,126' Name='stop' Size='96,22' Style='/Styles.New.buttons.hud.style'>Stop</Button>
	</Page>
	<Page BackgroundOpacity='0.88' BackgroundTint='#16373A' Location='292,245' Name='EntertainerRebornScriptPicker' RStyleDefault='/Styles.New.pill_med.rs_default' Size='440,164' UserMovable='true' Visible='false'>
		<Data Name='CodeData' cancel='cancel' refresh='refresh' script='script' start='start' status='status'/>
		<Text Font='bold_13' LocalText='Start Music from Script' Location='12,10' Name='title' PalText='header' Size='410,19'>Start Music from Script</Text>
		<ComboBox BackgroundOpacity='0.70' ButtonStyle='/Styles.New.buttons.combo_arrow.style' ButtonWidth='10' DataSource='ds' DropDownHeight='180' ListStyle='/Styles.list.combo.style_all' Location='12,38' Name='script' RStyleDefault='/Styles.New.combo.combo1.rs_default' ScrollbarStyle='/Styles.scrollbar.default.vHud3.style' Size='312,19' TextboxStyle='/Styles.textbox.combo.Style_new'><DataSource Name='ds'/></ComboBox>
		<Button LocalText='Refresh' Location='332,38' Name='refresh' Size='96,19' Style='/Styles.New.buttons.hud.style'>Refresh</Button>
		<Text Font='bold_12' LocalText='' Location='12,68' Name='status' Size='416,42'></Text>
		<Button LocalText='Start' Location='92,128' Name='start' Size='112,22' Style='/Styles.New.buttons.hud.style'>Start</Button><Button LocalText='Cancel' Location='236,128' Name='cancel' Size='112,22' Style='/Styles.New.buttons.hud.style'>Cancel</Button>
	</Page>
	<Page BackgroundOpacity='0.88' BackgroundTint='#16373A' Location='292,245' Name='EntertainerRebornSongPicker' RStyleDefault='/Styles.New.pill_med.rs_default' Size='440,164' UserMovable='true' Visible='false'>
		<Data Name='CodeData' cancel='cancel' song='song' start='start' status='status'/>
		<Text Font='bold_13' LocalText='Start MIDI to Flourish' Location='12,10' Name='title' PalText='header' Size='410,19'>Start MIDI to Flourish</Text>
		<ComboBox BackgroundOpacity='0.70' ButtonStyle='/Styles.New.buttons.combo_arrow.style' ButtonWidth='10' DataSource='ds' DropDownHeight='180' ListStyle='/Styles.list.combo.style_all' Location='12,38' Name='song' RStyleDefault='/Styles.New.combo.combo1.rs_default' ScrollbarStyle='/Styles.scrollbar.default.vHud3.style' Size='416,19' TextboxStyle='/Styles.textbox.combo.Style_new'><DataSource Name='ds'/></ComboBox>
		<Text Font='bold_12' LocalText='' Location='12,68' Name='status' Size='416,42'></Text>
		<Button LocalText='Start' Location='92,128' Name='start' Size='112,22' Style='/Styles.New.buttons.hud.style'>Start</Button><Button LocalText='Cancel' Location='236,128' Name='cancel' Size='112,22' Style='/Styles.New.buttons.hud.style'>Cancel</Button>
	</Page>
