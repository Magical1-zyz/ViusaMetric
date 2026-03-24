import os
import csv
import pymeshlab
from datetime import datetime


def _evaluate_pair(ref_path, opt_path):
    """
    核心几何计算引擎：加载一对模型、对齐包围盒并计算 Hausdorff 距离
    """
    ms = pymeshlab.MeshSet()
    try:
        # 载入基准模型 -> ID: 0
        ms.load_new_mesh(ref_path)
        # 载入待测模型 -> ID: 1
        ms.load_new_mesh(opt_path)

        # --- AABB 包围盒对齐校正 ---
        # 选中基准模型，平移至世界原点
        ms.set_current_mesh(0)
        # 使用更新后的枚举值 'Center on Layer BBox'
        ms.compute_matrix_from_translation(traslmethod='Center on Layer BBox')

        # 选中待测模型，平移至世界原点
        ms.set_current_mesh(1)
        # 使用更新后的枚举值 'Center on Layer BBox'
        ms.compute_matrix_from_translation(traslmethod='Center on Layer BBox')

        # --- 计算 Hausdorff 距离 ---
        # sampledmesh=1 (在简化模型上采样寻找表面), targetmesh=0 (映射到高精基准模型)
        distance_metrics = ms.get_hausdorff_distance(sampledmesh=1, targetmesh=0)

        return {
            "max": distance_metrics.get('max', 'Error'),
            "mean": distance_metrics.get('mean', 'Error'),
            "rms": distance_metrics.get('RMS', 'Error')
        }
    except Exception as e:
        print(f"    [计算异常] {ref_path} vs {opt_path} 错误信息: {e}")
        return {"max": "Failed", "mean": "Failed", "rms": "Failed"}


class BatchHausdorffEvaluator:
    """
    基于 PyMeshLab 的 3D 模型 Hausdorff 距离批量评估管线。
    """
    def __init__(self, ref_root_dir, opt_root_dir, output_csv_dir):
        """
        初始化评估管线
        :param ref_root_dir: 高精基准模型 (Reference) 的根目录
        :param opt_root_dir: 待评估模型 (Optimized) 的根目录
        :param output_csv_dir: 结果输出 CSV 的目标目录
        """
        self.ref_root = ref_root_dir
        self.opt_root = opt_root_dir
        self.output_csv_dir = output_csv_dir
        self.results = []

        # 支持的模型格式后缀
        self.supported_extensions = ('.obj', '.gltf')

    def _find_target_model(self, folder_path):
        """
        在指定文件夹内寻找支持的 3D 模型文件 (.obj, .gltf)
        注意：.bin 和 .mtl 将被 PyMeshLab 隐式关联加载，无需主动寻找
        """
        for file in os.listdir(folder_path):
            if file.lower().endswith(self.supported_extensions):
                return os.path.join(folder_path, file)
        return None

    def _export_to_csv(self):
        """
        将所有计算结果导出到 CSV 报表
        """
        if not os.path.exists(self.output_csv_dir):
            os.makedirs(self.output_csv_dir)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        csv_path = os.path.join(self.output_csv_dir, f"hausdorff_evaluation_{timestamp}.csv")

        headers = ['Model_Name', 'Max_Distance', 'Mean_Distance', 'RMS_Distance']

        with open(csv_path, mode='w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=headers)
            writer.writeheader()
            for row in self.results:
                writer.writerow(row)

        print(f"\n>>> 批处理完成！结果已成功导出至: {csv_path}")

    def run_pipeline(self):
        """
        执行完整管线：遍历匹配 -> 误差计算 -> 实时写入 CSV 报表
        """
        print(">>> 开始执行 Hausdorff 距离批量评估管线...")

        # 1. 提前准备好 CSV 文件并写入表头
        if not os.path.exists(self.output_csv_dir):
            os.makedirs(self.output_csv_dir)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        csv_path = os.path.join(self.output_csv_dir, f"hausdorff_evaluation_{timestamp}.csv")
        headers = ['Model_Name', 'Max_Distance', 'Mean_Distance', 'RMS_Distance']

        # 2. 打开文件，在整个循环过程中保持开启
        with open(csv_path, mode='w', newline='', encoding='utf-8') as f:
            writer = csv.DictWriter(f, fieldnames=headers)
            writer.writeheader()

            # 3. 开始遍历
            for model_name in os.listdir(self.ref_root):
                ref_folder = os.path.join(self.ref_root, model_name)
                if not os.path.isdir(ref_folder):
                    continue

                opt_folder = os.path.join(self.opt_root, model_name)
                if not os.path.exists(opt_folder):
                    print(f"[-] 跳过: {model_name} (在 opt 目录中未找到对应的文件夹)")
                    continue

                ref_file = self._find_target_model(ref_folder)
                opt_file = self._find_target_model(opt_folder)

                if not ref_file or not opt_file:
                    print(f"[-] 跳过: {model_name} (未找到有效的模型文件)")
                    continue

                print(f"[+] 正在评估: {model_name} ...")

                # 计算误差
                metrics = _evaluate_pair(ref_file, opt_file)

                # 4. 【关键改动】算完一个，立刻写入 CSV 并保存到硬盘
                row_data = {
                    'Model_Name': model_name,
                    'Max_Distance': metrics['max'],
                    'Mean_Distance': metrics['mean'],
                    'RMS_Distance': metrics['rms']
                }
                writer.writerow(row_data)
                f.flush()  # 强制将内存中的数据刷入硬盘，防止崩溃丢失

        print(f"\n>>> 批处理完成！结果已成功导出至: {csv_path}")


if __name__ == "__main__":
    # 在这里设定你的目录路径（绝对或相对路径均可）
    REF_DIR = "./assets/testmodel"          # 替换为你的 ref 根目录
    OPT_DIR = "./assets/optmodel/2_ablation/spectral_on_normal_off"          # 替换为你的 opt 根目录
    OUTPUT_DIR = "./output/2_ablation/spectral_on_normal_off" # 替换为你想要导出 csv 的目录

    # 实例化并运行管线
    evaluator = BatchHausdorffEvaluator(
        ref_root_dir=REF_DIR,
        opt_root_dir=OPT_DIR,
        output_csv_dir=OUTPUT_DIR
    )
    evaluator.run_pipeline()