linewidth = 2

# intvl = 1
intvl = 2
threads = [55, 110, 220, 440, 880, 1760, 3520, 7040, 14080]

result_dir = "~/lammps/results/latest"

data = [
  # {"omp_async" , "Async (omp)"       , "#1f77b4", "dot"    , "square-open" },
  {"sync"      , "Sync"              , "#1f77b4", "dot"    , "square-open" },
  {"async"     , "Async"             , "#ff7f0e", "dashdot", "diamond-open"},
  {"preemption", "Async + Preemption", "#2ca02c", "solid"  , "circle"      },
]

get_median_of_files = fn path ->
  path
  |> Path.expand()
  |> Path.wildcard()
  |> Enum.map(fn path ->
    File.stream!(path)
    |> Stream.map(&String.split/1)
    |> Stream.map(fn
      ["Loop", "time", "of", time | _] -> Float.parse(time)
      _                                -> nil
    end)
    |> Stream.filter(&!is_nil(&1))
    |> Enum.to_list()
    |> case do
      [{time, ""}] -> time
      _            -> nil
    end
  end)
  |> Statistics.median()
end

baseline = get_median_of_files.("#{result_dir}/no_analysis_*.out")

data
|> Enum.map(fn {prefix, title, color, dash, symbol} ->
  ys =
    Enum.map(threads, fn thread ->
      m = get_median_of_files.("#{result_dir}/#{prefix}_intvl_#{intvl}_thread_#{thread}_*.out")
      m / baseline - 1
    end)
  %{
    x: threads,
    y: ys,
    type: "scatter",
    name: title,
    line: %{
      color: color,
      width: linewidth,
      dash: dash,
    },
    marker: %{
      symbol: symbol,
      size:   12,
      line:   %{width: linewidth, color: color},
    },
  }
end)
|> PlotlyEx.plot(%{
  width:  400,
  height: 400,
  separators: ".",
  xaxis: %{
    type: "log",
    title: %{text: "# of analysis threads"},
    showline: true,
    tickvals: threads,
    exponentformat: "none",
  },
  yaxis: %{
    title: %{text: "Overhead"},
    showline: true,
    # range: [0, 0.85],
    range: [0, 0.45],
    dtick: 0.1,
  },
  font: %{
    size: 22,
  },
  legend: %{
      x: 0.4,
      y: 1.0,
  },
  margin: %{
    l: 70,
    r: 0,
    b: 100,
    t: 10,
    pad: 0,
  },
})
|> PlotlyEx.show()
