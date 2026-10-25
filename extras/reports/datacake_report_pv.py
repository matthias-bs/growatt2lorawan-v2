#!/usr/bin/env python3
###################################################################################################
# datacake_report_pv.py
#
# Robust CSV -> PDF report generator for PV inverter data (Datacake exports)
#
# created: 09/2024
###################################################################################################

import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.ticker as mticker
from matplotlib.backends.backend_pdf import PdfPages

COLUMNS = {
    'outputpower': 0,
    'energytoday': 2,
    'energytotal': 3,
}

# Use MonthLocator explicitly in plotting instead of replacing AutoDateLocator globally
# (replacing AutoDateLocator caused TypeError because MonthLocator doesn't accept
# the same constructor kwargs that AutoDateLocator does).

COLORS = {
    'outputpower': 'r',
    'energytoday': 'lime',
    'energytotal': 'limegreen',
}

WIDTH = 18  / 2.54
HEIGHT = 27 / 2.54

YEAR = 2026
# Directory containing the CSV files
src_dir = f"/home/mp/pCloudDrive/datacake/PV-Inverter"
pdf_file = f"/home/mp/pCloudDrive/datacake/PV-Inverter/pv_inverter_{YEAR}.pdf"
last_column = 4

# helper: normalize column name to token
def token_for_column(colname: str) -> str:
    s = str(colname).strip().lower()
    s = s.replace('\ufeff', '')  # BOM
    s = s.replace('\xa0', ' ')
    s = " ".join(s.split())
    # map common variants
    if 'time' in s or 'date' in s or 'timestamp' in s:
        return 'time'
    if 'output' in s and 'power' in s:
        return 'outputpower'
    if s in ('outputpower', 'output_power', 'output-power', 'output'):
        return 'outputpower'
    if 'energy' in s and 'today' in s:
        return 'energytoday'
    if s in ('energytoday', 'energy_today'):
        return 'energytoday'
    if 'energy' in s and ('total' in s or 'accum' in s):
        return 'energytotal'
    if s in ('energytotal', 'energy_total'):
        return 'energytotal'
    if 'temp' in s:
        return 'tempinverter'
    if 'status' in s:
        return 'status'
    # fallback to cleaned name
    return s.replace(' ', '')

# List all CSV files in the directory
csv_files = [f for f in os.listdir(src_dir) if f.lower().endswith('.csv')]

# Initialize an empty list to store DataFrames
dataframes = []

# Read each CSV file
for file in csv_files:
    file_path = os.path.join(src_dir, file)
    # read CSV (do not force parse dates yet)
    df = pd.read_csv(file_path, skiprows=0, dtype=str, keep_default_na=False)

    # Normalize column names and build mapping
    orig_cols = list(df.columns)
    mapped_names = []
    mapping = {}
    for c in orig_cols:
        tok = token_for_column(c)
        mapped_names.append(tok)
        mapping[c] = tok

    # rename columns to tokens (handle duplicates by suffixing index)
    # build unique names
    seen = {}
    unique_map = {}
    for orig, tok in mapping.items():
        cnt = seen.get(tok, 0)
        if cnt:
            tok_unique = f"{tok}_{cnt}"
        else:
            tok_unique = tok
        seen[tok] = cnt + 1
        unique_map[orig] = tok_unique

    df.rename(columns=unique_map, inplace=True)

    # Determine time column (prefer 'time' exactly, otherwise first col that contains 'time' token)
    time_col = None
    for c in df.columns:
        if c == 'time' or c.startswith('time'):
            time_col = c
            break
    if time_col is None:
        # fallback: first column
        time_col = df.columns[0]

    # Parse time column robustly
    # Try common format first, then fallback to pandas parser
    try:
        df[time_col] = pd.to_datetime(df[time_col], format='%a, %d %b %Y %H:%M:%S', errors='coerce')
    except Exception:
        df[time_col] = pd.to_datetime(df[time_col], errors='coerce')
        # Adaptive locator: for wide spans keep monthly major ticks, for month-size spans use weekly majors
        # and always enable daily minor ticks (drawn as vertical grid lines). This is a drop-in
        # replacement for mdates.MonthLocator() used later in plotting.
        class _AdaptiveMonthLocator:
            def __init__(self, *args, **kwargs):
                # underlying locators we will delegate to
                self._month_locator = mdates.MonthLocator()
                self._week_locator = mdates.DayLocator(interval=7)
                self._day_locator = mdates.DayLocator()  # daily minor ticks for grid
                self.axis = None

            def set_axis(self, axis):
                # called by matplotlib when the locator is attached to an axis
                # do NOT force a minor locator here (we decide minors per-plot later)
                self.axis = axis
                # forward to the underlying month locator so it can keep any internal state
                try:
                    self._month_locator.set_axis(axis)
                except Exception:
                    pass
                    def set_axis(self, axis):
                        # called by matplotlib when the locator is attached to an axis
                        self.axis = axis
                        try:
                            self._month_locator.set_axis(axis)
                        except Exception:
                            pass
                        try:
                            self._week_locator.set_axis(axis)
                        except Exception:
                            pass
                        try:
                            # keep day locator state for potential minor ticks use elsewhere
                            self._day_locator.set_axis(axis)
                        except Exception:
                            pass

                    def _to_datetimes(self, vmin, vmax):
                        try:
                            dt_vmin = mdates.num2date(vmin) if isinstance(vmin, (float, int)) else vmin
                            dt_vmax = mdates.num2date(vmax) if isinstance(vmax, (float, int)) else vmax
                            return dt_vmin, dt_vmax
                        except Exception:
                            return vmin, vmax

                    def tick_values(self, vmin, vmax):
                        # compute span in days robustly whether inputs are float datenums or datetimes
                        try:
                            if isinstance(vmin, (float, int)) and isinstance(vmax, (float, int)):
                                span_days = abs(vmax - vmin)
                            else:
                                dt_vmin, dt_vmax = self._to_datetimes(vmin, vmax)
                                span_days = abs(mdates.date2num(dt_vmax) - mdates.date2num(dt_vmin))
                        except Exception:
                            span_days = 999

                        # choose weekly majors for short spans (<= ~40 days), monthly otherwise
                        if span_days <= 40:
                            try:
                                return self._week_locator.tick_values(vmin, vmax)
                            except Exception:
                                dt_vmin, dt_vmax = self._to_datetimes(vmin, vmax)
                                return self._week_locator.tick_values(dt_vmin, dt_vmax)
                        else:
                            try:
                                return self._month_locator.tick_values(vmin, vmax)
                            except Exception:
                                dt_vmin, dt_vmax = self._to_datetimes(vmin, vmax)
                                return self._month_locator.tick_values(dt_vmin, dt_vmax)

                    def __call__(self, *args):
                        # support both calling conventions: __call__(vmin, vmax) or __call__() where axis provides view
                        if len(args) >= 2:
                            return self.tick_values(args[0], args[1])
                        if getattr(self, 'axis', None) is not None:
                            vmin, vmax = self.axis.get_view_interval()
                            return self.tick_values(vmin, vmax)
                        return []
        # Replace MonthLocator factory with our adaptive locator so existing plotting calls
        # (ax.xaxis.set_major_locator(mdates.MonthLocator())) behave as desired.
        mdates.MonthLocator = _AdaptiveMonthLocator
    # If parse failed for many rows, try alternative formats by letting pandas infer
    if df[time_col].isna().sum() > 0 and df[time_col].notna().sum() == 0:
        df[time_col] = pd.to_datetime(df[time_col].astype(str), errors='coerce')
        # Monkey-patch DateFormatter so wide-range axes (yearly overview) drop the time part
        _OriginalDateFormatter = mdates.DateFormatter
        class _AdaptiveDateFormatter(_OriginalDateFormatter):
            def __call__(self, x, pos=None):
                try:
                    axis = getattr(self, 'axis', None)
                    if axis is not None:
                        vmin, vmax = axis.get_view_interval()
                        # vmin/vmax are matplotlib datenum floats -> difference in days
                        span_days = abs(vmax - vmin)
                        # if the span is more than about one month, remove time from format
                        if span_days >= 32:
                            orig_fmt = self.fmt
                            # remove common time directives
                            fmt_no_time = orig_fmt.replace(' %H:%M', '').replace('%H:%M', '').strip()
                            self.fmt = fmt_no_time
                            try:
                                return super().__call__(x, pos)
                            finally:
                                self.fmt = orig_fmt
                except Exception:
                    # fall back to default behavior on any error
                    pass
                return super().__call__(x, pos)

        mdates.DateFormatter = _AdaptiveDateFormatter
    # Clean numeric columns: any column that is not the time column and not obviously status/text
    for c in df.columns:
        if c == time_col:
            continue
        # skip obvious text columns
        if 'status' in c or c.startswith('status'):
            continue
        # replace comma decimal separator and whitespace then convert
        df[c] = df[c].astype(str) \
            .str.replace(',', '.', regex=False) \
            .str.replace('\xa0', '', regex=False) \
            .str.replace(' ', '', regex=False) \
            .replace({'': None, 'nan': None, 'None': None})
        df[c] = pd.to_numeric(df[c], errors='coerce')

    # Ensure there is a standard 'time' column name for concatenation
    if time_col != 'time':
        df.rename(columns={time_col: 'time'}, inplace=True)
        time_col = 'time'

    dataframes.append(df)

# Combine all DataFrames into a single DataFrame (ignore empty)
if not dataframes:
    raise SystemExit("No CSV files found")

combined_df = pd.concat(dataframes, ignore_index=True, sort=False)

# Ensure 'time' exists
if 'time' not in combined_df.columns:
    # try first column
    combined_df.rename(columns={combined_df.columns[0]: 'time'}, inplace=True)

# Drop rows with invalid time and set index
combined_df['time'] = pd.to_datetime(combined_df['time'], errors='coerce')
combined_df.dropna(subset=['time'], inplace=True)
combined_df.set_index('time', inplace=True)

# Sort the DataFrame by the index (time)
combined_df.sort_index(inplace=True)

# Print the combined DataFrame (debug)
print(combined_df.head())

# Optionally drop rows where all monitored numeric columns are NaN
numeric_cols = [c for c in combined_df.columns if c not in ('status', 'tempinverter') and combined_df[c].dtype.kind in 'bifc']
if numeric_cols:
    combined_df.dropna(subset=numeric_cols, how='all', inplace=True)

# The rest of the plotting code expects columns by name -> map to column indices dynamically
# Build a column -> index mapping
col_index = {name: i for i, name in enumerate(combined_df.columns)}

def get_col_index(name):
    return col_index.get(name, None)

# Create a PdfPages object to save the figures
with PdfPages(pdf_file) as pdf:
    def title_page(plt, pdf, title, font_size=24):
        fig, ax = plt.subplots(figsize=(WIDTH, HEIGHT))
        ax.text(0.5, 0.5, title, transform=ax.transAxes, fontsize=font_size, ha='center', va='center')
        ax.axis('off')
        pdf.savefig(fig)
        plt.close(fig)

    def plot_data(plt, pdf, df, column_names, title, xlabel, ylabels, colors, avg_label, avg_colors,
                  monthly_ticks=False):
        # resolve available column indices
        available_cols = [get_col_index(n) for n in column_names]
        # filter out missing
        avail = [i for i in available_cols if i is not None]
        if not avail:
            return
        fig, axes = plt.subplots(len(avail), 1, figsize=(WIDTH, HEIGHT), layout="tight")
        for idx_i, ci in enumerate(avail):
            ax = axes if len(avail) == 1 else axes[idx_i]
            ax.plot(df.index, df.iloc[:, ci], label=ylabels[idx_i], color=colors[idx_i])
            if xlabel:
                ax.set_xlabel(xlabel)
            ax.set_ylabel(ylabels[idx_i])
            # Adaptive major locator: use weekly majors for short spans, monthly for long spans
            class _AdaptiveMajorLocator(mticker.Locator):
                """Locator that chooses weekly majors for short spans and monthly majors for long spans."""
                def __init__(self):
                    super().__init__()
                    self._month = mdates.MonthLocator()
                    self._week = mdates.DayLocator(interval=7)
                    self.axis = None

                def set_axis(self, axis):
                    # matplotlib calls this to attach the locator to an axis
                    self.axis = axis
                    try:
                        self._month.set_axis(axis)
                        self._week.set_axis(axis)
                    except Exception:
                        pass

                def _to_datetimes(self, vmin, vmax):
                    """Normalize vmin/vmax to datetime objects for locators that expect dates."""
                    try:
                        dt_vmin = mdates.num2date(vmin) if isinstance(vmin, (float, int)) else vmin
                        dt_vmax = mdates.num2date(vmax) if isinstance(vmax, (float, int)) else vmax
                        return dt_vmin, dt_vmax
                    except Exception:
                        return vmin, vmax

                def tick_values(self, vmin, vmax):
                    # compute span in days robustly whether inputs are float datenums or datetimes
                    try:
                        if isinstance(vmin, (float, int)) and isinstance(vmax, (float, int)):
                            span_days = abs(vmax - vmin)
                        else:
                            dt_vmin, dt_vmax = self._to_datetimes(vmin, vmax)
                            span_days = abs(mdates.date2num(dt_vmax) - mdates.date2num(dt_vmin))
                    except Exception:
                        # fallback conservative value
                        span_days = 999

                    # choose locator and ensure we pass expected types to underlying locator
                    if span_days <= 40:
                        try:
                            return self._week.tick_values(vmin, vmax)
                        except Exception:
                            dt_vmin, dt_vmax = self._to_datetimes(vmin, vmax)
                            return self._week.tick_values(dt_vmin, dt_vmax)
                    else:
                        try:
                            return self._month.tick_values(vmin, vmax)
                        except Exception:
                            dt_vmin, dt_vmax = self._to_datetimes(vmin, vmax)
                            return self._month.tick_values(dt_vmin, dt_vmax)

                def __call__(self, *args):
                    # support both calling conventions: __call__(vmin, vmax) or __call__() where axis provides view
                    if len(args) >= 2:
                        return self.tick_values(args[0], args[1])
                    if getattr(self, 'axis', None) is not None:
                        vmin, vmax = self.axis.get_view_interval()
                        return self.tick_values(vmin, vmax)
                    return []

            if monthly_ticks:
                ax.xaxis.set_major_locator(mdates.MonthLocator(interval=1))
                ax.xaxis.set_minor_locator(mdates.WeekdayLocator(byweekday=mdates.MO))
                ax.tick_params(axis='x', which='minor', length=3)
            else:
                # Daily minor ticks for vertical grid lines
                ax.xaxis.set_minor_locator(mdates.DayLocator())
                # set adaptive major locator instance
                ax.xaxis.set_major_locator(_AdaptiveMajorLocator())

            # Adaptive formatter: hide time when span is wide
            class _AdaptiveDateFormatter(mdates.DateFormatter):
                def set_axis(self, axis):
                    super().set_axis(axis)
                    self.axis = axis
                def __call__(self, x, pos=None):
                    try:
                        if getattr(self, 'axis', None) is not None:
                            vmin, vmax = self.axis.get_view_interval()
                            span_days = abs(vmax - vmin)
                            fmt = '%d-%m-%y' if span_days >= 32 else '%d-%m-%y %H:%M'
                            old = self.fmt
                            self.fmt = fmt
                            out = super().__call__(x, pos)
                            self.fmt = old
                            return out
                    except Exception:
                        pass
                    return super().__call__(x, pos)

            ax.xaxis.set_major_formatter(_AdaptiveDateFormatter('%d-%m-%y %H:%M'))
            if idx_i < len(avail) - 1:
                ax.set_xticklabels([])
            if avg_colors[idx_i]:
                daily_avg = df.iloc[:, ci].resample('D').mean()
                ax.plot(daily_avg.index, daily_avg, label=f'{avg_label} {ylabels[idx_i]}', color=avg_colors[idx_i], linestyle='--', linewidth=2)
            ax.legend()
            # Show monthly grid lines only in the yearly overview.
            ax.grid(True, which='major', axis='x')
            if monthly_ticks:
                ax.grid(False, which='minor', axis='x')
            else:
                ax.grid(True, which='minor', axis='x', linestyle=':', linewidth=0.5, color='lightgray')
            plt.setp(ax.xaxis.get_majorticklabels(), rotation=45)
        # save and close the figure so it ends up in the PDF
        pdf.savefig(fig)
        plt.close(fig)
        # Force all date formatters to show date only (no time)
        _OriginalDateFormatter = mdates.DateFormatter
        class _DateOnlyFormatter(_OriginalDateFormatter):
            def __call__(self, x, pos=None):
                # temporarily force a date-only format, call the original formatter, then restore
                orig = getattr(self, 'fmt', None)
                try:
                    self.fmt = '%d-%m-%y'
                    return super().__call__(x, pos)
                finally:
                    if orig is not None:
                        self.fmt = orig
                    # For very wide spans (yearly overview) disable daily minor grid lines
                    # and remove the daily minor locator so a vertical line isn't drawn for each day.
                    try:
                        axis = getattr(self, 'axis', None)
                        if axis is not None:
                            try:
                                vmin, vmax = axis.get_view_interval()
                                span_days = abs(vmax - vmin)
                                if span_days >= 300:
                                    try:
                                        axis.grid(False, which='minor', axis='x')
                                    except Exception:
                                        pass
                                    try:
                                        axis.xaxis.set_minor_locator(mdates.NullLocator())
                                    except Exception:
                                        pass
                            except Exception:
                                pass
                    except Exception:
                        pass
                    if orig is not None:
                        self.fmt = orig
                    # Annotate period total energy (prefer energytotal diff, fallback to summed daily max of energytoday)
                    try:
                        # If this formatter is being used on a wide span (e.g. the yearly overview),
                        # disable minor vertical grid lines to reduce clutter.
                        axis = getattr(self, 'axis', None)
                        if axis is not None:
                            try:
                                vmin, vmax = axis.get_view_interval()
                                span_days = abs(vmax - vmin)
                                # treat spans of ~300 days or more as "yearly overview"
                                if span_days >= 300:
                                    try:
                                        axis.grid(False, which='minor', axis='x')
                                    except Exception:
                                        # fallback: remove minor locator so minor ticks won't be drawn
                                        try:
                                            axis.xaxis.set_minor_locator(mdates.NullLocator())
                                        except Exception:
                                            pass
                            except Exception:
                                pass
                        total_wh = None
                        total_wh = None
                        if 'energytotal' in df.columns and df['energytotal'].dropna().shape[0] >= 2:
                            s = df['energytotal'].dropna()
                            total_wh = float(s.iloc[-1]) - float(s.iloc[0])
                        elif 'energytoday' in df.columns and df['energytoday'].dropna().shape[0] > 0:
                            # energytoday typically resets each day -> take daily maxima and sum
                            daily_max = df['energytoday'].resample('D').max()
                            total_wh = float(daily_max.dropna().sum())
                        if total_wh is not None and not pd.isna(total_wh):
                            total_kwh = total_wh / 1000.0
                            txt = f"Period total: {total_kwh:.2f} kWh"
                            # place text on the figure (top-right) with light box for readability
                            try:
                                fig.text(0.98, 0.98, txt, ha='right', va='top',
                                         fontsize=10, bbox=dict(facecolor='white', alpha=0.8, edgecolor='none'))
                            except Exception:
                                # fallback to annotating first axis
                                ax0 = axes if not isinstance(axes, (list, tuple)) else axes[0]
                                ax0.annotate(txt, xy=(0.99, 0.99), xycoords='axes fraction',
                                             ha='right', va='top', fontsize=9,
                                             bbox=dict(facecolor='white', alpha=0.8, edgecolor='none'))
                    except Exception:
                        # don't fail plotting if annotation logic errors
                        pass
        mdates.DateFormatter = _DateOnlyFormatter
    title_page(plt, pdf, f'PV-Inverter {YEAR}')
    title_page(plt, pdf, 'Yearly Overview', 18)

    # build requested column name list (use normalized names)
    requested = ['outputpower', 'energytoday', 'energytotal']
    # if different naming was used, combined_df columns already normalized
    plot_data(plt, pdf, combined_df, requested,
              'PV-Inverter', None,
              ['Power [W]', 'Energy today [Wh]', 'Energy total [Wh]'],
              [COLORS['outputpower'], COLORS['energytoday'], COLORS['energytotal']],
              'Daily average', [None, None, None], monthly_ticks=True)

    title_page(plt, pdf, 'Monthly Reports', 18)

    # Group the data by month
    combined_df['Month'] = combined_df.index.to_period('M')

    # Create a separate figure for each month
    for month, month_df in combined_df.groupby('Month'):
        title_page(plt, pdf, str(month), 16)
        plot_data(plt, pdf, month_df, requested,
                  f'PV-Inverter {month}', None,
                  ['Power [W]', 'Energy today [Wh]', 'Energy total [Wh]'],
                  [COLORS['outputpower'], COLORS['energytoday'], COLORS['energytotal']],
                  'Daily average', [None, None, None])
