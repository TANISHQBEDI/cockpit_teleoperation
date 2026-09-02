import pygame

def main():
    pygame.init()

    # Initialize the joystick module
    pygame.joystick.init()

    # Check for available joysticks
    joystick_count = pygame.joystick.get_count()

    if joystick_count == 0:
        print("No joysticks found.")
        return

    # Select the first joystick
    joystick = pygame.joystick.Joystick(0)
    joystick.init()

    print(f"Initialized joystick: {joystick.get_name()}")

    try:
        while True:
            # Handle events
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    return

            # Read joystick axes, buttons, and hat
            num_axes = joystick.get_numaxes()
            num_buttons = joystick.get_numbuttons()

            axes_values = [joystick.get_axis(i) for i in range(num_axes)]
            button_values = [joystick.get_button(i) for i in range(num_buttons)]
            hat_value = joystick.get_hat(0)

            # Print the joystick values
            print("Axes values:", axes_values)
            print("Button values:", button_values)
            print("Hat value:", hat_value)

    except KeyboardInterrupt:
        pass

    finally:
        # Quit the joystick module
        joystick.quit()
        pygame.quit()

if __name__ == "__main__":
    main()
