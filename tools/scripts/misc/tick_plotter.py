import matplotlib.pyplot as plt
import sys
import os
import numpy as np
from scipy import stats

def plot_ticks(file_path):
  ticks_65 = []
  ticks_67 = []
  line_numbers_65 = []
  line_numbers_67 = []

  with open(file_path, 'r') as file:
    for i, line in enumerate(file):
      parts_vertical_split = line.split('|')
      parts = parts_vertical_split[1].split(',')

      tick = int(parts[0].strip().split(':')[1].strip())
      addr = int(parts[2].strip().split(':')[1].strip())
      if addr == 65:
        ticks_65.append(tick)
        line_numbers_65.append(i + 1)
      elif addr == 67:
        ticks_67.append(tick)
        line_numbers_67.append(i + 1)

  # Calculate differences and average difference for addr: 65
  if ticks_65:
    plt.figure()  # Create a new figure
    differences_65 = np.diff(ticks_65)

    for i in range(1, len(ticks_65)):
      if abs(differences_65[i - 1]) > 10000:
          plt.axvline(x=line_numbers_65[i], color='red')
      plt.plot([line_numbers_65[i - 1], line_numbers_65[i]], [ticks_65[i - 1], ticks_65[i]], color='blue')

    plt.xlabel('Line Number')
    plt.ylabel('Tick')
    plt.title('Tick Plot for ' + file_path + ' (addr: 65)')
    plt.legend()

    # Save the plot as a JPEG file in the same directory as the input file
    dir_name = os.path.dirname(file_path)
    base_name = os.path.basename(file_path)
    plot_file_name = os.path.splitext(base_name)[0] + '_ticks_plot_65.jpg'
    plot_file_path = os.path.join(dir_name, plot_file_name)
    plt.savefig(plot_file_path)

  # Calculate differences and mode difference for addr: 67
  if ticks_67:
    plt.figure()  # Create a new figure
    differences_67 = np.diff(ticks_67)

    for i in range(1, len(ticks_67)):
      if abs(differences_67[i - 1]) > 10000:
          plt.axvline(x=line_numbers_67[i], color='red')
      plt.plot([line_numbers_67[i - 1], line_numbers_67[i]], [ticks_67[i - 1], ticks_67[i]], color='blue')

    plt.xlabel('Line Number')
    plt.ylabel('Tick')
    plt.title('Tick Plot for ' + file_path + ' (addr: 67)')
    plt.legend()

    # Save the plot as a JPEG file in the same directory as the input file
    plot_file_name = os.path.splitext(base_name)[0] + '_ticks_plot_67.jpg'
    plot_file_path = os.path.join(dir_name, plot_file_name)
    plt.savefig(plot_file_path)

def find_power_logs(directory):
    power_logs = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file == 'power.log':
                power_logs.append(os.path.join(root, file))
    return power_logs

# Get directory from command-line arguments
directory = sys.argv[1]

# Find all power.log files in the directory and its subdirectories
power_logs = find_power_logs(directory)

# Plot ticks for each power.log file
for power_log in power_logs:
    plot_ticks(power_log)

plt.show()  # Show all the plots at once