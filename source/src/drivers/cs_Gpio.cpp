#include <ble/cs_Nordic.h>
#include <cfg/cs_AutoConfig.h>
#include <drivers/cs_Gpio.h>
#include <events/cs_EventDispatcher.h>

static void gpioEventHandler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t polarity) {
	Gpio::getInstance().registerEvent(pin);
}

Gpio& Gpio::getInstance() {
	static Gpio instance;
	return instance;
}

Gpio::Gpio(): EventListener() {
	EventDispatcher::getInstance().addListener(this);
}

/*
 * Initialize GPIO with the board configuration. If there are button pins defined (as is the case on dev. boards) we
 * can actually control these from other modules. We limit here the number of pins that can be controlled to not be
 * accidentally driving the relay or the IGBTs.
 */
void Gpio::init(const boards_config_t & board) {

	int activePins = 0;
	for (int i = 0; i < GPIO_INDEX_COUNT; i++) {
		_pins[i].event = false;
		if (board.pinGpio[i] != PIN_NONE) {
			activePins++;
		}
	}
	for (int i = 0; i < BUTTON_COUNT; i++) {
		_pins[GPIO_INDEX_COUNT + i].event = false;
		if (board.pinButton[i] != PIN_NONE) {
			activePins++;
		}
	}
	for (int i = 0; i < LED_COUNT; i++) {
		_pins[GPIO_INDEX_COUNT + BUTTON_COUNT + i].event = false;
		if (board.pinLed[i] != PIN_NONE) {
			activePins++;
		}
	}

	_initialized = true;
	_boardConfig = &board;
	LOGi("Configured %i GPIO pins", activePins);

	// Hardcoding for 3-phase current monitoring
	for (int i = 0;  i < 6; ++i) {
		configure(i, GpioDirection::OUTPUT, GpioPullResistor::GPR_NONE, GpioPolarity::GP_NONE);
	}

	uint8_t low = 0;
	uint8_t high = 1;
	write(0, low);
	write(1, low);
	write(2, low);
	write(3, low);
	write(4, low);
	write(5, high);
}

bool Gpio::pinExists(uint8_t pin_index) {
	if (!_initialized) {
		LOGi("GPIO driver not initialized");
		return false;
	}
	if (pin_index >= TOTAL_PIN_COUNT) {
		LOGi("Pin index %i out of max pin range (max %i)", pin_index, TOTAL_PIN_COUNT);
		return false;
	}
	if (pin_index < GPIO_INDEX_COUNT) {
		if (_boardConfig->pinGpio[pin_index] != PIN_NONE) {
			return true;
		}
	}
	else if (pin_index < GPIO_INDEX_COUNT + BUTTON_COUNT) {
		if (_boardConfig->pinButton[pin_index - GPIO_INDEX_COUNT] != PIN_NONE) {
			return true;
		}
	}
	else {
		if (_boardConfig->pinLed[pin_index - GPIO_INDEX_COUNT - BUTTON_COUNT] != PIN_NONE) {
			return true;
		}
	}
	LOGi("Pin index %i not defined for this board", pin_index);
	return false;
}

pin_t Gpio::getPin(uint8_t pin_index) {

	if (!pinExists(pin_index)) {
		return PIN_NONE;
	}
	if (pin_index < GPIO_INDEX_COUNT) {
		return _boardConfig->pinGpio[pin_index];
	}
	else if (pin_index < GPIO_INDEX_COUNT + BUTTON_COUNT) {
		return _boardConfig->pinButton[pin_index - GPIO_INDEX_COUNT];
	}
	else {
		return _boardConfig->pinLed[pin_index - GPIO_INDEX_COUNT - BUTTON_COUNT];
	}
}

bool Gpio::isLedPin(uint8_t pin_index) {
	if (!pinExists(pin_index)) {
		return false;
	}
	else {
		return (pin_index >= GPIO_INDEX_COUNT + BUTTON_COUNT);
	}
}

void Gpio::configure(uint8_t pin_index, GpioDirection direction, GpioPullResistor pull, GpioPolarity polarity) {

	pin_t pin = getPin(pin_index);
	if (pin == PIN_NONE) {
		LOGi("Can't configure pin with pin index %i", pin_index);
		return;
	}

	nrf_gpio_pin_pull_t nrf_pull;
	switch(pull) {
		case GpioPullResistor::GPR_NONE:
			nrf_pull = NRF_GPIO_PIN_NOPULL;
			LOGi("Set pin %i with index %i to use no pull-up", pin, pin_index);
			break;
		case GpioPullResistor::UP:
			nrf_pull = NRF_GPIO_PIN_PULLUP;
			LOGi("Set pin %i with index %i to use pull-up", pin, pin_index);
			break;
		case GpioPullResistor::DOWN:
			nrf_pull = NRF_GPIO_PIN_PULLDOWN;
			LOGi("Set pin %i with index %i to use pull-down", pin, pin_index);
			break;
		default:
			LOGw("Huh? No such pull registor construction exists");
			return;
	}

	switch(direction) {
		case GpioDirection::INPUT:
			LOGi("Configure pin %i as input", pin);
			nrf_gpio_cfg_input(pin, nrf_pull);
			break;
		case GpioDirection::OUTPUT:
			LOGi("Configure pin %i as output", pin);
			nrf_gpio_cfg_output(pin);
			break;
		case GpioDirection::SENSE:
			// enable GPIOTE in general
			uint32_t err_code;
			if (!nrfx_gpiote_is_init()) {
				err_code = nrfx_gpiote_init();
				if (err_code != NRF_SUCCESS) {
					LOGw("Error initializing GPIOTE");
					return;
				}
			}

			nrfx_gpiote_in_config_t config;
			switch(polarity) {
				case GpioPolarity::HITOLO:
					config.sense = NRF_GPIOTE_POLARITY_HITOLO;
					break;
				case GpioPolarity::LOTOHI:
					config.sense = NRF_GPIOTE_POLARITY_LOTOHI;
					break;
				case GpioPolarity::TOGGLE:
					config.sense = NRF_GPIOTE_POLARITY_TOGGLE;
					break;
				default:
					LOGw("Huh? No such polarity exists");
					return;
			}
			config.pull = nrf_pull;
			config.hi_accuracy = true;
			config.is_watcher = false;
			config.skip_gpio_setup = false;

			LOGi("Register pin %i using polarity %i with event handler", pin, polarity);

			err_code = nrfx_gpiote_in_init(pin, &config, gpioEventHandler);
			if (err_code != NRF_SUCCESS) {
				LOGw("Cannot initialize GPIOTE on this pin");
				return;
			}
			nrfx_gpiote_in_event_enable(pin, true);

			break;
		default:
			LOGw("Unknown pin action");
			break;
	}
}

/*
 * We just write, we assume the user has already configured the pin as output and with desired pull-up, etc.
 */
void Gpio::write(uint8_t pin_index, uint8_t *buf, uint8_t & length) {

	pin_t pin = getPin(pin_index);
	if (pin == PIN_NONE) {
		LOGi("Can't write pin with pin index %i", pin_index);
		return;
	}

	// TODO: limit length
	for (int i = 0; i < length; ++i) {
		// Invert value to write if leds are inverted
		if (isLedPin(pin_index) && _boardConfig->flags.ledInverted) {
			buf[i] = !buf[i];
		}
		LOGi("Write value %i to pin %i (pin index %i)", buf[i], pin, pin_index);
		nrf_gpio_pin_write(pin, buf[i]);
	}
}

/*
 * We just write, we assume the user has already configured the pin as output and with desired pull-up, etc.
 */
void Gpio::write(uint8_t pin_index, uint8_t value) {

	pin_t pin = getPin(pin_index);
	if (pin == PIN_NONE) {
		LOGi("Can't write pin with pin index %i", pin_index);
		return;
	}

	// Invert value to write if leds are inverted
	if (isLedPin(pin_index) && _boardConfig->flags.ledInverted) {
		value = !value;
	}
	LOGi("Write value %i to pin %i (pin index %i)", value, pin, pin_index);
	nrf_gpio_pin_write(pin, value);
}

void Gpio::read(uint8_t pin_index, uint8_t *buf, uint8_t & length) {

	pin_t pin = getPin(pin_index);
	if (pin == PIN_NONE) {
		LOGi("Can't read pin with pin index %i", pin_index);
		return;
	}

	// TODO: limit length
	for (int i = 0; i < length; ++i) {
		buf[i] = nrf_gpio_pin_read(pin);
		LOGi("Read value %i from pin %i (pin index %i)", buf[i], pin, pin_index);
	}
}

/*
 * Called from interrupt service routine, only write which pin is fired and return immediately.
 */
void Gpio::registerEvent(pin_t pin) {

	LOGd("GPIO event on pin %i", pin);
	for (uint8_t i = 0; i < GPIO_INDEX_COUNT; ++i) {
		if (_boardConfig->pinGpio[i] == pin) {
			_pins[i].event = true;
			return;
		}
	}
	for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
		if (_boardConfig->pinButton[i] == pin) {
			_pins[GPIO_INDEX_COUNT + i].event = true;
			return;
		}
	}
	for (uint8_t i = 0; i < LED_COUNT; ++i) {
		if (_boardConfig->pinLed[i] == pin) {
			_pins[GPIO_INDEX_COUNT + BUTTON_COUNT + i].event = true;
			return;
		}
	}
}

void Gpio::tick() {
	for (uint8_t i = 0; i < TOTAL_PIN_COUNT; ++i) {
		if (_pins[i].event) {
			_pins[i].event = false;
			TYPIFY(EVT_GPIO_UPDATE) gpio;
			// we send back the pin index, not the pin number
			gpio.pin_index = i;
			gpio.length = 0;
			LOGd("Send GPIO event at index %i", gpio.pin_index);
			event_t event(CS_TYPE::EVT_GPIO_UPDATE, &gpio, sizeof(gpio));
			EventDispatcher::getInstance().dispatch(event);
		}
	}
}

void Gpio::handleEvent(event_t& event) {
	switch(event.type) {
		case CS_TYPE::EVT_GPIO_INIT: {
			LOGi("Configure pin in GPIO module");
			TYPIFY(EVT_GPIO_INIT) gpio = *(TYPIFY(EVT_GPIO_INIT)*)event.data;
			GpioPolarity polarity = (GpioPolarity)gpio.polarity;
			GpioDirection direction = (GpioDirection)gpio.direction;
			GpioPullResistor pull = (GpioPullResistor)gpio.pull;
			configure(gpio.pin_index, direction, pull, polarity);
			break;
		}
		case CS_TYPE::EVT_GPIO_WRITE: {
			TYPIFY(EVT_GPIO_WRITE) gpio = *(TYPIFY(EVT_GPIO_WRITE)*)event.data;
			if (gpio.buf == NULL) {
				LOGw("Buffer is null");
				return;
			}
			write(gpio.pin_index, gpio.buf, gpio.length);
			break;
		}
		case CS_TYPE::EVT_GPIO_READ: {
			TYPIFY(EVT_GPIO_READ) gpio = *(TYPIFY(EVT_GPIO_READ)*)event.data;
			read(gpio.pin_index, gpio.buf, gpio.length);
			break;
		}
		case CS_TYPE::EVT_TICK: {
			tick();
			break;
		}
		default:
			break;
	}
}
